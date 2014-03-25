Degraded-First scheduling is a MapReduce Scheduler improving the
performance of degraded tasks in erasure-coded storage. 

According to default scheduling scheduler, the tasks processing
lost blocks (degraded tasks) are put to the end of map phase. 
This is not good because in this way, there will be congestion.
However, we distribute degraded tasks evenly in the map phase
by making sure the ratio of launched degraded tasks equals to 
that of normal tasks. 

We also provide locality preservation and rack awareness 
heuristic to further improve the performances.
