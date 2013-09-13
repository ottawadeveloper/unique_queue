<?php

/**
 * UniqueQueueFilter::handleOperation() should return this code (or no code)
 * if the queue should not perform any operation.
 */
define('UNIQUE_QUEUE_FILTER_OP_NOOP', 0);

/**
 * UniqueQueueFilter::handleOperation() should return this code if the 
 * queueItem should be updated.
 */
define('UNIQUE_QUEUE_FILTER_OP_UPDATE', 1);

/**
 * UniqueQueueFilter::handleOperation() should return this code if the queueItem
 * should be deleted.
 */
define('UNIQUE_QUEUE_FILTER_OP_DELETE', 2);

/**
 * Sometimes, routines might want to edit items from the queue, based on
 * a set of parameters. This provides an interface for them to implement that
 * can be passed into a UniqueQueue in order to manipulate results.
 */
interface UniqueQueueFilter {
  
  /**
   * Performs whatever work needs to be done by this filter.
   * 
   * Returns one of the constants starting with UNIQUE_QUEUE_FILTER_OP_ which
   * determines what the queue will do with the item after you're done with it
   * (nothing is the default, can also update or delete).
   * 
   * @param stdClass $queueItem The queue item that is currently being 
   * examined.
   * 
   * @return int Returns an op code for what the queue should do.
   */
  function handleOperation(stdClass &$queueItem);
  
}