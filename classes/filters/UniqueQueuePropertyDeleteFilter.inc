<?php
/**
 * @file
 * Contains the UniqueQueuePropertyDeleteFilter class definition.
 */

/**
 * Deletes items from a unique queue if their property matches a given one.
 */
class UniqueQueuePropertyDeleteFilter implements UniqueQueueFilter {
  
  /**
   * The name of the property to find
   * 
   * @var string
   */
  private $name = '';
  
  /**
   * The value that the property with $name must have to match.
   * @var mixed
   */
  private $value = '';
  
  /**
   * Whether or not to use strict matching.
   * 
   * @var boolean
   */
  private $strict = TRUE;
  
  /**
   * Constructor.
   * 
   * @param string $propertyName
   *   The name of the property to use.
   * @param mixed $propertyValue
   *   The value that the property must match.
   * @param boolean $strict
   *   Whether or not to use strict matching (eg !== over ==). Defaults to TRUE.
   */
  public function __construct($propertyName, $propertyValue, $strict = TRUE) {
    $this->name = $propertyName;
    $this->value = $propertyValue;
    $this->strict = $strict;
  }
  
  /**
   * Checks to see if $queueItem should be deleted, based on whether or not it
   * has a $name property that matches $value.
   * 
   * @param stdClass $queueItem
   *   The queue item to check.
   * 
   * @return int
   *   One of the following values:
   *   - UNIQUE_QUEUE_FILTER_OP_DELETE: If the queueItem should be deleted.
   *   - UNIQUE_QUEUE_FILTER_OP_NOOP: If nothing should happen to it.
   */
  public function handleOperation(stdClass &$queueItem) {
    if (isset($queueItem->data[$this->name])) {
      if ($this->strict && ($queueItem->data[$this->name] === $this->value)) {
        return UNIQUE_QUEUE_FILTER_OP_DELETE;
      }
      if ((!($this->strict)) && ($queueItem->data[$this->name] == $this->value)) {
        return UNIQUE_QUEUE_FILTER_OP_DELETE;
      }
    }
    return UNIQUE_QUEUE_FILTER_OP_NOOP;
  }
  
}
