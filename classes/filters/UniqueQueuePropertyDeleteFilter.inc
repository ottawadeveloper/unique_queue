<?php

/**
 * Deletes items from a unique queue if their property matches a given one.
 */
class UniqueQueuePropertyDeleteFilter implements UniqueQueueFilter {
  
  private $name = '';
  private $value = '';
  private $strict = TRUE;
  
  public function __construct($propertyName, $propertyValue, $strict = TRUE) {
    $this->name = $propertyName;
    $this->value = $propertyValue;
    $this->strict = $strict;
  }
  
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
