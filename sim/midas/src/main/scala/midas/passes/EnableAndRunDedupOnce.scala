// See LICENSE for license details.

package midas.passes

import firrtl.{Transform, DependencyAPIMigration, CircuitState}

class EnableAndRunDedupOnce extends Transform with DependencyAPIMigration {
  override def prerequisites = firrtl.stage.Forms.Resolved
  override def optionalPrerequisites = Nil
  override def optionalPrerequisiteOf = Nil
  override def invalidates(a: Transform) = false

  override def execute(state: CircuitState): CircuitState = {
    val (noDedupAnnos, otherAnnos) = state.annotations.partition {
      case firrtl.transforms.NoCircuitDedupAnnotation => true
      case _ => false
    }
    val deduped = (new firrtl.transforms.DedupModules).execute(state.copy(annotations = otherAnnos))
    deduped.copy(annotations = deduped.annotations ++ noDedupAnnos)
  }
}
