#!/usr/bin/env Rscript

################################################################################
# Interactive Shiny Dashboard - James Law Window Scaling
################################################################################
#
# Real-time web-based dashboard showing live experiment progress
#
# Usage:
#   ./live_dashboard.R [port]
#
# Example:
#   ./live_dashboard.R 8080    # Run on port 8080
#
# Then open browser to: http://localhost:8080
################################################################################

suppressPackageStartupMessages({
  library(shiny)
  library(readr)
  library(dplyr)
  library(ggplot2)
  library(plotly)
  library(DT)
})

# Configuration
args <- commandArgs(trailingOnly = TRUE)
PORT <- if(length(args) > 0) as.numeric(args[1]) else 3838

# Get experiment directory (assumes script is run from experiment root or scripts/)
get_experiment_dir <- function() {
  # Try to determine if we're in scripts/ or experiment root
  if (file.exists("results")) {
    # We're in experiment root
    return(".")
  } else if (file.exists("../results_run_one")) {
    # We're in scripts/
    return("..")
  } else {
    # Fallback: use script location
    args <- commandArgs(trailingOnly = FALSE)
    file_arg <- grep("^--file=", args, value = TRUE)
    if (length(file_arg) > 0) {
      script_dir <- dirname(sub("^--file=", "", file_arg))
      return(file.path(script_dir, ".."))
    }
    return(".")
  }
}

EXPERIMENT_DIR <- get_experiment_dir()

# Paths (relative to experiment directory)
RESULTS_CSV <- file.path(EXPERIMENT_DIR, "results/raw/window_sweep_results.csv")
HB_DIR <- file.path(EXPERIMENT_DIR, "results/raw/hb")

################################################################################
# Data Loading Functions
################################################################################

load_summary_data <- function() {
  if (!file.exists(RESULTS_CSV)) {
    return(data.frame())
  }

  tryCatch({
    df <- read_csv(RESULTS_CSV, show_col_types = FALSE,
                   col_types = cols(.default = col_guess()))

    if (nrow(df) > 0) {
      df <- df %>% filter(!is.na(window_size))
    }

    return(df)
  }, error = function(e) {
    return(data.frame())
  })
}

load_latest_heartbeat <- function() {
  if (!dir.exists(HB_DIR)) {
    return(NULL)
  }

  hb_files <- list.files(HB_DIR, pattern = "^run-.*\\.csv$", full.names = TRUE)
  if (length(hb_files) == 0) {
    return(NULL)
  }

  latest_file <- hb_files[which.max(file.mtime(hb_files))]

  tryCatch({
    hb_data <- read_csv(latest_file, show_col_types = FALSE)
    return(list(
      file = basename(latest_file),
      data = hb_data,
      mtime = file.mtime(latest_file)
    ))
  }, error = function(e) {
    return(NULL)
  })
}

get_heartbeat_files <- function() {
  if (!dir.exists(HB_DIR)) {
    return(character(0))
  }

  hb_files <- list.files(HB_DIR, pattern = "^run-.*\\.csv$", full.names = FALSE)
  return(sort(hb_files, decreasing = TRUE))
}

load_heartbeat_file <- function(filename) {
  filepath <- file.path(HB_DIR, filename)
  if (!file.exists(filepath)) {
    return(NULL)
  }

  tryCatch({
    read_csv(filepath, show_col_types = FALSE)
  }, error = function(e) {
    NULL
  })
}

################################################################################
# UI
################################################################################

ui <- fluidPage(
  tags$head(
    tags$style(HTML("
      .main-header {
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        color: white;
        padding: 20px;
        border-radius: 10px;
        margin-bottom: 20px;
        box-shadow: 0 4px 6px rgba(0,0,0,0.1);
      }
      .metric-box {
        background: white;
        border-radius: 8px;
        padding: 15px;
        margin: 10px 0;
        box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        border-left: 4px solid #667eea;
      }
      .metric-value {
        font-size: 2em;
        font-weight: bold;
        color: #667eea;
      }
      .metric-label {
        color: #666;
        font-size: 0.9em;
        text-transform: uppercase;
      }
      .progress-container {
        background: #f5f5f5;
        border-radius: 10px;
        padding: 20px;
        margin: 20px 0;
      }
      .status-running { color: #28a745; }
      .status-waiting { color: #ffc107; }
      .status-complete { color: #17a2b8; }
    "))
  ),

  div(class = "main-header",
      h1("🔬 James Law Window Scaling", style = "margin: 0;"),
      h3("Live Experiment Dashboard", style = "margin: 5px 0 0 0; opacity: 0.9;")
  ),

  fluidRow(
    column(3,
           div(class = "metric-box",
               div(class = "metric-label", "Progress"),
               div(class = "metric-value", textOutput("progress_text")),
               div(style = "margin-top: 10px;",
                   plotOutput("progress_bar", height = "30px"))
           )
    ),
    column(3,
           div(class = "metric-box",
               div(class = "metric-label", "Mean K Statistic"),
               div(class = "metric-value", textOutput("mean_k"))
           )
    ),
    column(3,
           div(class = "metric-box",
               div(class = "metric-label", "Target Deviation"),
               div(class = "metric-value", textOutput("k_deviation"))
           )
    ),
    column(3,
           div(class = "metric-box",
               div(class = "metric-label", "Status"),
               div(class = "metric-value", uiOutput("status_text"))
           )
    )
  ),

  tabsetPanel(
    type = "tabs",

    # Overview Tab
    tabPanel("📊 Overview",
             fluidRow(
               column(12,
                      h3("K Statistic Evolution"),
                      plotlyOutput("k_evolution_plot", height = "400px")
               )
             ),
             fluidRow(
               column(6,
                      h3("K by Window Size"),
                      plotlyOutput("k_by_window_plot", height = "400px")
               ),
               column(6,
                      h3("Performance Distribution"),
                      plotlyOutput("performance_plot", height = "400px")
               )
             )
    ),

    # Live Heartbeat Tab
    tabPanel("💓 Live Heartbeat",
             fluidRow(
               column(12,
                      h3("Latest Heartbeat Trace"),
                      selectInput("hb_metric", "Metric:",
                                  choices = c("K Approximate" = "K_approx",
                                              "K Deviation" = "K_deviation",
                                              "Window Width" = "window_width",
                                              "Hot Words" = "hot_word_count",
                                              "L8 Mode" = "l8_mode"),
                                  selected = "K_approx"),
                      plotlyOutput("heartbeat_trace", height = "500px")
               )
             ),
             fluidRow(
               column(12,
                      h4("Heartbeat File Browser"),
                      selectInput("hb_file", "Select File:", choices = NULL),
                      plotlyOutput("hb_file_plot", height = "400px")
               )
             )
    ),

    # Data Table Tab
    tabPanel("📋 Data Table",
             h3("Summary Results"),
             DTOutput("summary_table")
    ),

    # Statistics Tab
    tabPanel("📈 Statistics",
             fluidRow(
               column(6,
                      h3("K Distribution"),
                      plotlyOutput("k_histogram", height = "400px")
               ),
               column(6,
                      h3("Window Size Distribution"),
                      plotlyOutput("window_histogram", height = "400px")
               )
             ),
             fluidRow(
               column(12,
                      h3("Detailed Statistics"),
                      verbatimTextOutput("detailed_stats")
               )
             )
    )
  ),

  hr(),
  div(style = "text-align: center; color: #999; padding: 20px;",
      "Auto-refresh: Every 5 seconds | ",
      textOutput("last_update", inline = TRUE)
  )
)

################################################################################
# Server
################################################################################

server <- function(input, output, session) {

  # Reactive data with auto-refresh
  summary_data <- reactiveVal(data.frame())
  hb_data <- reactiveVal(NULL)
  hb_files_list <- reactiveVal(character(0))

  # Auto-refresh every 5 seconds
  observe({
    invalidateLater(5000)

    # Load summary data
    df <- load_summary_data()
    summary_data(df)

    # Load latest heartbeat
    hb <- load_latest_heartbeat()
    hb_data(hb)

    # Update heartbeat file list
    files <- get_heartbeat_files()
    hb_files_list(files)
    updateSelectInput(session, "hb_file", choices = files)
  })

  # Progress metrics
  output$progress_text <- renderText({
    df <- summary_data()
    if (nrow(df) == 0) return("0 / 360")
    sprintf("%d / 360", nrow(df))
  })

  output$progress_bar <- renderPlot({
    df <- summary_data()
    pct <- if(nrow(df) == 0) 0 else (nrow(df) / 360) * 100

    ggplot(data.frame(x = 1, y = pct)) +
      geom_col(aes(x = x, y = y), fill = "#667eea", width = 1) +
      ylim(0, 100) +
      theme_void() +
      theme(plot.margin = margin(0,0,0,0))
  }, bg = "transparent")

  output$mean_k <- renderText({
    df <- summary_data()
    if (nrow(df) == 0) return("—")
    sprintf("%.4f", mean(df$K_statistic, na.rm = TRUE))
  })

  output$k_deviation <- renderText({
    df <- summary_data()
    if (nrow(df) == 0) return("—")
    mean_k <- mean(df$K_statistic, na.rm = TRUE)
    sprintf("%.4f", abs(mean_k - 1.0))
  })

  output$status_text <- renderUI({
    df <- summary_data()
    if (nrow(df) == 0) {
      tags$span(class = "status-waiting", "WAITING")
    } else if (nrow(df) >= 360) {
      tags$span(class = "status-complete", "COMPLETE")
    } else {
      tags$span(class = "status-running", "RUNNING")
    }
  })

  # K Evolution Plot
  output$k_evolution_plot <- renderPlotly({
    df <- summary_data()

    if (nrow(df) == 0) {
      return(plotly_empty() %>%
               layout(title = "Waiting for data..."))
    }

    df <- df %>%
      arrange(timestamp) %>%
      mutate(
        run_number = row_number(),
        k_cumulative_mean = cummean(K_statistic)
      )

    plot_ly(df) %>%
      add_trace(x = ~run_number, y = ~K_statistic,
                type = 'scatter', mode = 'markers',
                name = 'K (individual)',
                marker = list(size = 4, opacity = 0.3)) %>%
      add_trace(x = ~run_number, y = ~k_cumulative_mean,
                type = 'scatter', mode = 'lines',
                name = 'K (cumulative mean)',
                line = list(width = 3, color = '#667eea')) %>%
      add_trace(x = c(0, max(df$run_number)), y = c(1, 1),
                type = 'scatter', mode = 'lines',
                name = 'Target (K = 1.0)',
                line = list(dash = 'dash', color = 'red')) %>%
      layout(
        title = "K Statistic Accumulation Over Time",
        xaxis = list(title = "Run Number"),
        yaxis = list(title = "K Statistic"),
        hovermode = 'closest'
      )
  })

  # K by Window Size
  output$k_by_window_plot <- renderPlotly({
    df <- summary_data()

    if (nrow(df) == 0) {
      return(plotly_empty() %>%
               layout(title = "Waiting for data..."))
    }

    df_summary <- df %>%
      group_by(window_size) %>%
      summarise(
        mean_k = mean(K_statistic, na.rm = TRUE),
        se_k = sd(K_statistic, na.rm = TRUE) / sqrt(n()),
        n = n(),
        .groups = "drop"
      )

    plot_ly(df_summary) %>%
      add_trace(x = ~window_size, y = ~mean_k,
                type = 'scatter', mode = 'markers+lines',
                error_y = list(array = ~se_k),
                marker = list(size = 10, color = ~n,
                              colorscale = 'Viridis',
                              showscale = TRUE,
                              colorbar = list(title = "Replicates"))) %>%
      add_trace(x = range(df_summary$window_size), y = c(1, 1),
                type = 'scatter', mode = 'lines',
                line = list(dash = 'dash', color = 'red'),
                name = 'Target') %>%
      layout(
        title = "Mean K by Window Size",
        xaxis = list(title = "Window Size (bytes)", type = "log"),
        yaxis = list(title = "Mean K Statistic"),
        showlegend = FALSE
      )
  })

  # Performance Plot
  output$performance_plot <- renderPlotly({
    df <- summary_data()

    if (nrow(df) == 0 || !"ns_per_word" %in% names(df)) {
      return(plotly_empty() %>%
               layout(title = "Waiting for data..."))
    }

    plot_ly(df) %>%
      add_trace(x = ~window_size, y = ~ns_per_word,
                type = 'scatter', mode = 'markers',
                marker = list(size = 6, opacity = 0.5, color = '#764ba2')) %>%
      layout(
        title = "Performance: ns/word vs Window Size",
        xaxis = list(title = "Window Size (bytes)", type = "log"),
        yaxis = list(title = "Nanoseconds per Word"),
        hovermode = 'closest'
      )
  })

  # Heartbeat Trace
  output$heartbeat_trace <- renderPlotly({
    hb <- hb_data()

    if (is.null(hb)) {
      return(plotly_empty() %>%
               layout(title = "No heartbeat data available"))
    }

    metric_col <- input$hb_metric

    plot_ly(hb$data) %>%
      add_trace(x = ~tick_number, y = ~get(metric_col),
                type = 'scatter', mode = 'lines',
                line = list(width = 1, color = '#667eea')) %>%
      layout(
        title = sprintf("Latest: %s (%d ticks)", hb$file, nrow(hb$data)),
        xaxis = list(title = "Tick Number"),
        yaxis = list(title = metric_col),
        hovermode = 'closest'
      )
  })

  # Heartbeat File Browser
  output$hb_file_plot <- renderPlotly({
    req(input$hb_file)

    hb_data_file <- load_heartbeat_file(input$hb_file)

    if (is.null(hb_data_file)) {
      return(plotly_empty() %>%
               layout(title = "Could not load file"))
    }

    plot_ly(hb_data_file) %>%
      add_trace(x = ~tick_number, y = ~K_approx,
                type = 'scatter', mode = 'lines',
                name = 'K Approx',
                line = list(color = 'blue')) %>%
      add_trace(x = ~tick_number, y = ~K_deviation,
                type = 'scatter', mode = 'lines',
                name = 'K Deviation',
                line = list(color = 'orange')) %>%
      layout(
        title = sprintf("%s (%d ticks)", input$hb_file, nrow(hb_data_file)),
        xaxis = list(title = "Tick Number"),
        yaxis = list(title = "Value"),
        hovermode = 'closest'
      )
  })

  # Summary Table
  output$summary_table <- renderDT({
    df <- summary_data()

    if (nrow(df) == 0) {
      return(datatable(data.frame(Message = "No data available")))
    }

    df_display <- df %>%
      select(timestamp, window_size, replicate, K_statistic,
             K_deviation, entropy, ns_per_word) %>%
      arrange(desc(timestamp))

    datatable(df_display,
              options = list(
                pageLength = 25,
                scrollX = TRUE,
                order = list(list(0, 'desc'))
              )) %>%
      formatRound(c('K_statistic', 'K_deviation', 'entropy', 'ns_per_word'), 4)
  })

  # K Histogram
  output$k_histogram <- renderPlotly({
    df <- summary_data()

    if (nrow(df) == 0) {
      return(plotly_empty() %>%
               layout(title = "Waiting for data..."))
    }

    plot_ly(df, x = ~K_statistic) %>%
      add_histogram(marker = list(color = '#667eea')) %>%
      add_trace(x = c(1, 1), y = c(0, max(table(cut(df$K_statistic, 30)))),
                type = 'scatter', mode = 'lines',
                line = list(dash = 'dash', color = 'red', width = 2),
                name = 'Target') %>%
      layout(
        title = "Distribution of K Statistics",
        xaxis = list(title = "K Statistic"),
        yaxis = list(title = "Frequency"),
        showlegend = FALSE
      )
  })

  # Window Histogram
  output$window_histogram <- renderPlotly({
    df <- summary_data()

    if (nrow(df) == 0) {
      return(plotly_empty() %>%
               layout(title = "Waiting for data..."))
    }

    window_counts <- df %>%
      count(window_size)

    plot_ly(window_counts) %>%
      add_bars(x = ~window_size, y = ~n,
               marker = list(color = '#764ba2')) %>%
      layout(
        title = "Runs per Window Size",
        xaxis = list(title = "Window Size (bytes)", type = "log"),
        yaxis = list(title = "Number of Runs")
      )
  })

  # Detailed Statistics
  output$detailed_stats <- renderText({
    df <- summary_data()

    if (nrow(df) == 0) {
      return("No data available")
    }

    stats <- sprintf(
"Experimental Statistics
========================
Total Runs:          %d / 360 (%.1f%%)
Unique Windows:      %d

K Statistic
-----------
Mean:                %.6f
Median:              %.6f
Std Dev:             %.6f
CV:                  %.2f%%
Min:                 %.6f
Max:                 %.6f
Target Deviation:    %.6f

James Law Status
----------------
Target K:            1.0000
Current Mean K:      %.6f
Deviation from 1.0:  %.6f (%.1f%%)
Status:              %s
",
      nrow(df), (nrow(df) / 360) * 100,
      length(unique(df$window_size)),
      mean(df$K_statistic, na.rm = TRUE),
      median(df$K_statistic, na.rm = TRUE),
      sd(df$K_statistic, na.rm = TRUE),
      100 * sd(df$K_statistic, na.rm = TRUE) / mean(df$K_statistic, na.rm = TRUE),
      min(df$K_statistic, na.rm = TRUE),
      max(df$K_statistic, na.rm = TRUE),
      abs(mean(df$K_statistic, na.rm = TRUE) - 1.0),
      mean(df$K_statistic, na.rm = TRUE),
      abs(mean(df$K_statistic, na.rm = TRUE) - 1.0),
      abs(mean(df$K_statistic, na.rm = TRUE) - 1.0) * 100,
      if(abs(mean(df$K_statistic, na.rm = TRUE) - 1.0) < 0.1) "✓ CONFIRMED" else "✗ NOT CONFIRMED"
    )

    return(stats)
  })

  # Last update timestamp
  output$last_update <- renderText({
    invalidateLater(5000)
    format(Sys.time(), "%H:%M:%S")
  })
}

################################################################################
# Launch App
################################################################################

cat("\n")
cat("╔═══════════════════════════════════════════════════════════════╗\n")
cat("║   James Law Window Scaling - Interactive Dashboard           ║\n")
cat("╚═══════════════════════════════════════════════════════════════╝\n")
cat("\n")
cat(sprintf("Starting Shiny server on port %d...\n", PORT))
cat(sprintf("\n📊 Open your browser to: http://localhost:%d\n\n", PORT))
cat("Features:\n")
cat("  • Real-time experiment progress\n")
cat("  • Live K statistic evolution\n")
cat("  • Interactive heartbeat traces\n")
cat("  • Auto-refresh every 5 seconds\n")
cat("\nPress Ctrl+C to stop the server.\n\n")

# Run the app
shinyApp(ui = ui, server = server, options = list(port = PORT, host = "0.0.0.0"))