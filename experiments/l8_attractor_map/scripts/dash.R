#!/usr/bin/env Rscript

suppressPackageStartupMessages({
  library(shiny)
  library(data.table)
  library(ggplot2)
})

# ------------------------------------------------------------------------------
# Locate experiment root (.. from scripts/dash.R)
# ------------------------------------------------------------------------------
get_script_dir <- function() {
  args <- commandArgs(trailingOnly = FALSE)
  file_arg <- "--file="
  match <- grep(file_arg, args)
  if (length(match) > 0) {
    normalizePath(dirname(sub(file_arg, "", args[match])))
  } else {
    # fallback: working dir
    getwd()
  }
}

SCRIPT_DIR       <- get_script_dir()
EXPERIMENT_ROOT  <- normalizePath(file.path(SCRIPT_DIR, ".."))
LATEST_FILE      <- file.path(EXPERIMENT_ROOT, ".latest_results_dir")

if (!file.exists(LATEST_FILE)) {
  stop("No .latest_results_dir found. Run ./scripts/setup.sh and ./scripts/run.sh first.")
}

RESULTS_DIR      <- normalizePath(readLines(LATEST_FILE, warn = FALSE)[1])
HB_CSV           <- file.path(RESULTS_DIR, "heartbeat_all.csv")

if (!file.exists(HB_CSV)) {
  stop(sprintf("Heartbeat CSV not found at: %s\nDid run.sh create heartbeat_all.csv?", HB_CSV))
}

# ------------------------------------------------------------------------------
# Helper: load heartbeat data safely (with basic downcasting)
# ------------------------------------------------------------------------------
load_heartbeat <- function(path) {
  # fread is fast; keep types mostly numeric
  dt <- fread(
    path,
    header = FALSE,
    col.names = c(
      "tick_number",
      "elapsed_ns",
      "tick_interval_ns",
      "cache_hits_delta",
      "bucket_hits_delta",
      "word_executions_delta",
      "hot_word_count",
      "avg_word_heat",
      "window_width",
      "predicted_label_hits",
      "estimated_jitter_ns"
    ),
    showProgress = FALSE
  )

  # Ensure numeric types are sane
  num_cols <- c(
    "tick_number",
    "elapsed_ns",
    "tick_interval_ns",
    "cache_hits_delta",
    "bucket_hits_delta",
    "word_executions_delta",
    "hot_word_count",
    "avg_word_heat",
    "window_width",
    "predicted_label_hits",
    "estimated_jitter_ns"
  )

  for (nm in num_cols) {
    if (!is.numeric(dt[[nm]])) {
      suppressWarnings(dt[[nm]] <- as.numeric(dt[[nm]]))
    }
  }

  dt
}

# ------------------------------------------------------------------------------
# Shiny UI
# ------------------------------------------------------------------------------
metric_choices <- c(
  "avg_word_heat",
  "hot_word_count",
  "window_width",
  "estimated_jitter_ns",
  "tick_interval_ns",
  "word_executions_delta",
  "cache_hits_delta",
  "bucket_hits_delta",
  "predicted_label_hits"
)

axis_time_choices <- c(
  "tick_number",
  "elapsed_ns"
)

ui <- fluidPage(
  tags$head(
    tags$style(HTML("
      body { background-color: #111; color: #ddd; }
      .navbar, .well, .panel { background-color: #222 !important; color: #ddd !important; }
      .form-control, .selectize-input { background-color: #333 !important; color: #ddd !important; }
      .tabbable > .nav > li > a { background-color: #222 !important; color: #aaa !important; }
      .tabbable > .nav > li.active > a { background-color: #444 !important; color: #fff !important; }
    "))
  ),
  titlePanel("StarForth L8 Attractor — Heartbeat Dashboard"),
  sidebarLayout(
    sidebarPanel(
      width = 3,
      h4("Data Source"),
      verbatimTextOutput("path_info"),

      hr(),
      h4("Sampling / Window"),
      selectInput(
        "time_axis",
        "Time axis:",
        choices = axis_time_choices,
        selected = "elapsed_ns"
      ),
      numericInput(
        "max_points",
        "Max points per plot (downsample):",
        value = 50000,
        min = 1000,
        max = 500000,
        step = 5000
      ),
      sliderInput(
        "window_frac",
        "Fraction of trajectory to show:",
        min = 0,
        max = 1,
        value = c(0, 1),
        step = 0.01
      ),
      checkboxInput(
        "auto_refresh",
        "Auto-refresh from CSV (every 5s)",
        value = TRUE
      ),
      actionButton("force_reload", "Force Reload Now"),

      hr(),
      h4("Phase Portrait Downsample"),
      numericInput(
        "phase_points",
        "Max points in phase plot:",
        value = 25000,
        min = 1000,
        max = 100000,
        step = 5000
      )
    ),
    mainPanel(
      width = 9,
      tabsetPanel(
        tabPanel(
          "Time Series",
          br(),
          fluidRow(
            column(
              12,
              selectInput(
                "ts_metrics",
                "Metrics to plot (time series, up to 4):",
                choices = metric_choices,
                selected = c("avg_word_heat", "window_width", "estimated_jitter_ns"),
                multiple = TRUE
              )
            )
          ),
          plotOutput("ts_plot", height = "550px")
        ),
        tabPanel(
          "Phase Portraits",
          br(),
          fluidRow(
            column(
              6,
              selectInput(
                "phase_x",
                "X axis metric:",
                choices = metric_choices,
                selected = "avg_word_heat"
              )
            ),
            column(
              6,
              selectInput(
                "phase_y",
                "Y axis metric:",
                choices = metric_choices,
                selected = "window_width"
              )
            )
          ),
          plotOutput("phase_plot", height = "550px")
        )
      )
    )
  )
)

# ------------------------------------------------------------------------------
# Shiny Server
# ------------------------------------------------------------------------------
server <- function(input, output, session) {

  output$path_info <- renderText({
    paste0(
      "Experiment root: ", EXPERIMENT_ROOT, "\n",
      "Results dir:     ", RESULTS_DIR, "\n",
      "Heartbeat CSV:   ", HB_CSV
    )
  })

  # Reactive file reader with polling
  hb_data <- reactivePoll(
    5000, # check every 5s
    session,
    checkFunc = function() {
      info <- file.info(HB_CSV)
      if (is.na(info$mtime)) return(Sys.time())
      info$mtime
    },
    valueFunc = function() {
      if (!file.exists(HB_CSV)) return(NULL)
      load_heartbeat(HB_CSV)
    }
  )

  # Manual reload button overrides polling
  observeEvent(input$force_reload, {
    hb_data()  # just trigger reactive
  })

  # Optionally disable polling
  observe({
    if (!isTRUE(input$auto_refresh)) {
      # do nothing, hb_data will only update on force_reload
    } else {
      # reactivePoll already polling; no extra work needed
      invisible(NULL)
    }
  })

  # Filter + downsample data for plotting
  filtered_data <- reactive({
    dt <- hb_data()
    if (is.null(dt) || nrow(dt) == 0) return(NULL)

    # window_frac defines [start, end] as quantiles over row indices
    fr <- input$window_frac
    start_frac <- max(0, min(fr[1], 1))
    end_frac   <- max(0, min(fr[2], 1))
    if (end_frac <= start_frac) {
      end_frac <- start_frac + 0.01
      if (end_frac > 1) end_frac <- 1
    }

    n <- nrow(dt)
    idx_start <- max(1L, as.integer(start_frac * n))
    idx_end   <- max(1L, as.integer(end_frac * n))
    if (idx_end <= idx_start) idx_end <- min(n, idx_start + 1L)

    dt_win <- dt[idx_start:idx_end]

    # Downsample for plotting
    max_pts <- input$max_points
    if (!is.null(max_pts) && max_pts > 0 && nrow(dt_win) > max_pts) {
      # uniform subsample
      idx <- round(seq(1, nrow(dt_win), length.out = max_pts))
      dt_win <- dt_win[idx]
    }

    dt_win
  })

  # Time series plot
  output$ts_plot <- renderPlot({
    dt <- filtered_data()
    if (is.null(dt) || nrow(dt) == 0) {
      ggplot() + theme_minimal(base_size = 14) +
        labs(title = "No data loaded yet")
    } else {
      metrics <- intersect(input$ts_metrics, metric_choices)
      if (length(metrics) == 0) return(NULL)

      time_col <- input$time_axis
      if (!time_col %in% names(dt)) time_col <- "elapsed_ns"

      # Melt for multi-metric plotting
      mdt <- melt(
        dt,
        id.vars = time_col,
        measure.vars = metrics,
        variable.name = "metric",
        value.name = "value"
      )

      ggplot(mdt, aes_string(x = time_col, y = "value", color = "metric")) +
        geom_line(alpha = 0.8, linewidth = 0.4) +
        theme_minimal(base_size = 14) +
        theme(
          plot.background = element_rect(fill = "#111111", color = NA),
          panel.background = element_rect(fill = "#111111", color = NA),
          legend.position = "bottom",
          legend.title = element_blank(),
          text = element_text(color = "#dddddd"),
          axis.text = element_text(color = "#dddddd"),
          legend.background = element_rect(fill = "#111111", color = NA)
        ) +
        labs(
          title = "Heartbeat Time Series",
          x = time_col,
          y = "Value"
        )
    }
  })

  # Phase portrait plot
  output$phase_plot <- renderPlot({
    dt <- filtered_data()
    if (is.null(dt) || nrow(dt) == 0) {
      ggplot() + theme_minimal(base_size = 14) +
        labs(title = "No data loaded yet")
    } else {
      x_col <- input$phase_x
      y_col <- input$phase_y

      if (!(x_col %in% names(dt)) || !(y_col %in% names(dt))) return(NULL)

      # Additional downsample for phase portrait
      max_phase <- input$phase_points
      if (!is.null(max_phase) && max_phase > 0 && nrow(dt) > max_phase) {
        idx <- round(seq(1, nrow(dt), length.out = max_phase))
        dt <- dt[idx]
      }

      ggplot(dt, aes_string(x = x_col, y = y_col)) +
        geom_point(alpha = 0.3, size = 0.7) +
        theme_minimal(base_size = 14) +
        theme(
          plot.background = element_rect(fill = "#111111", color = NA),
          panel.background = element_rect(fill = "#111111", color = NA),
          text = element_text(color = "#dddddd"),
          axis.text = element_text(color = "#dddddd")
        ) +
        labs(
          title = "Phase Portrait",
          x = x_col,
          y = y_col
        )
    }
  })
}

# ------------------------------------------------------------------------------
# Run the app
# ------------------------------------------------------------------------------
app <- shinyApp(ui = ui, server = server)
runApp(app, host = "0.0.0.0", port = 3838)
