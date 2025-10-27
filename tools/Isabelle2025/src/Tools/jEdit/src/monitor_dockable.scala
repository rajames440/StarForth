/*  Title:      Tools/jEdit/src/monitor_dockable.scala
    Author:     Makarius

Monitor for runtime statistics.
*/

package isabelle.jedit


import isabelle._

import java.awt.BorderLayout

import scala.collection.immutable.Queue
import scala.swing.TextField
import scala.swing.event.ValueChanged

import org.jfree.chart.ChartPanel
import org.jfree.data.xy.XYSeriesCollection

import org.gjt.sp.jedit.View


class Monitor_Dockable(view: View, position: String) extends Dockable(view, position) {
  /* chart data -- owned by GUI thread */

  private var statistics = Queue.empty[Properties.T]
  private var statistics_length = 0

  private def add_statistics(stats: Properties.T): Unit = {
    statistics = statistics.appended(stats)
    statistics_length += 1
    limit_data.text match {
      case Value.Int(limit) =>
        while (statistics_length > limit) {
          statistics = statistics.dequeue._2
          statistics_length -= 1
        }
      case _ =>
    }
  }
  private def clear_statistics(): Unit = {
    statistics = Queue.empty
    statistics_length = 0
  }

  private var data_name = ML_Statistics.all_fields.head.title
  private val chart = ML_Statistics.empty.chart(null, Nil)
  private val data = chart.getXYPlot.getDataset.asInstanceOf[XYSeriesCollection]

  private def update_chart(): Unit = {
    ML_Statistics.all_fields.find(_.title == data_name) match {
      case None =>
      case Some(fields) => ML_Statistics(statistics.toList).update_data(data, fields.names)
    }
  }

  private val input_delay =
    Delay.first(PIDE.session.input_delay, gui = true) { update_chart() }

  private val update_delay =
    Delay.first(PIDE.session.chart_delay, gui = true) { update_chart() }


  /* controls */

  private val select_data =
    new GUI.Selector(ML_Statistics.all_fields.map(p => GUI.Selector.item(p._1))) {
      tooltip = "Select visualized data collection"
      override def changed(): Unit = { data_name = selection.item.toString; update_chart() }
    }

  private val limit_data = new TextField("200", 5) {
    tooltip = "Limit for accumulated data"
    verifier = {
      case Value.Int(x) => x > 0
      case _ => false
    }
    reactions += { case ValueChanged(_) => input_delay.invoke() }
  }

  private val reset_data = new GUI.Button("Reset") {
    tooltip = "Reset accumulated data"
    override def clicked(): Unit = { clear_statistics(); update_chart() }
  }

  private val full_gc = new GUI.Button("GC") {
    tooltip = "Full garbage collection of ML heap"
    override def clicked(): Unit = PIDE.session.protocol_command("ML_Heap.full_gc")
  }

  private val share_common_data = new GUI.Button("Sharing") {
    tooltip = "Share common data of ML heap"
    override def clicked(): Unit = PIDE.session.protocol_command("ML_Heap.share_common_data")
  }

  private val controls =
    Wrap_Panel(List(select_data, limit_data, reset_data, full_gc, share_common_data))


  /* layout */

  set_content(new ChartPanel(chart))
  add(controls.peer, BorderLayout.NORTH)


  /* main */

  private val main =
    Session.Consumer[Session.Runtime_Statistics](getClass.getName) {
      stats =>
        add_statistics(stats.props)
        update_delay.invoke()
    }

  override def init(): Unit = {
    PIDE.session.runtime_statistics += main
  }

  override def exit(): Unit = {
    PIDE.session.runtime_statistics -= main
  }
}
