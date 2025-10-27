/*  Title:      Pure/Concurrent/counter.scala
    Author:     Makarius

Synchronized counter for unique identifiers < 0.

NB: ML ticks forwards, JVM ticks backwards.
*/

package isabelle


object Counter {
  type ID = Long
  def make(): Counter = new Counter
}

final class Counter private {
  private var count: Counter.ID = 0

  def apply(): Counter.ID = synchronized {
    require(count > Long.MinValue, "counter overflow")
    count -= 1
    count
  }

  override def toString: String = count.toString
}
