<?php
/**
 * @file
 * Contains the DBUniqueQueue class definition.
 */

/**
 * An implementation of UniqueQueueInterface that uses database tables to handle
 * queue processing.
 *
 * @see UniqueQueueInterface
 */
class DBUniqueQueue extends UniqueQueue {

  /**
   * Sets the name of the queue.
   *
   * Protected constructor enforces a factory pattern on queues.
   *
   * @param string $name 
   *   Name of the queue
   */
  public function __construct($name) {
    parent::__construct($name);
  }

  /**
   * Stub implementation of DrupalQueueInterface::createQueue().
   */
  public function createQueue() {}

  /**
   * Frees all locks that have expired for this queue.
   */
  protected function freeLocks() {
    $q = db_update('unique_queue_items');
    $q->fields(array(
      'consumer_id' => NULL,
      'lock_expires' => NULL,
    ));
    $q->condition('lock_expires', time(), '<');
    $q->condition('queue_name', $this->queue_name);
    $q->execute();
  }

  /**
   * Creates a unique token for a unique queue item.
   *
   * In this implementation, we hash the value to ensure a consistent length.
   *
   * @param string $unique_token 
   *   The unique token
   * 
   * @return string 
   *   A hashed value
   */
  protected function generateUniqueToken($unique_token) {
    return hash('sha384', $unique_token);
  }

  /**
   * Called when the queue is created to ensure the name is tracked for
   * calls to release locks.
   */
  protected function registerQueue() {
    db_merge('unique_queue_queues')
        ->fields(array(
          'queue_name' => $this->queue_name,
        ))
        ->key(array(
          'queue_name' => $this->queue_name,
        ))
        ->execute();
  }

  /**
   * Releases an item that the worker could not process, so another worker can
   * come in and process it.
   *
   * @param object $item 
   *   An object as returned by claimItem().
   * 
   * @return boolean 
   *   TRUE if the object was released, otherwise FALSE
   */
  public function releaseItem($item) {
    return !!db_update('unique_queue_items')
        ->condition('item_id', $item->item_id)
        ->fields(array(
          'consumer_id' => NULL,
          'lock_expires' => NULL,
        ))
        ->execute();
  }

  /**
   * Tries to find an item with $unique_token in the DB.
   *
   * @param string $unique_token 
   *   The hashed unique token
   * @param int $priority 
   *   The new priority
   * @return string 
   *   One of the following values:
   *   - UNIQUE_QUEUE_MISSING: If the item is not in the DB
   *   - UNIQUE_QUEUE_UPDATE_REQUIRED: If the priority is too low
   *   - UNIQUE_QUEUE_MATCH: If the item exists and the priority is okay
   */
  protected function seek($unique_token, $priority = 0) {
    $q = db_select('unique_queue_items', 'uqi');
    $q->fields('uqi', array(
      'item_id',
      'priority'
    ));
    $q->condition('queue_name', $this->queue_name);
    $q->condition('item_uuid', $unique_token);
    $row = $q->execute()->fetchObject();
    if (empty($row)) {
      return UNIQUE_QUEUE_MISSING;
    } elseif ($row->priority < $priority) {
      return UNIQUE_QUEUE_UPDATE_REQUIRED;
    }
    return UNIQUE_QUEUE_MATCH;
  }

  /**
   * Adds an item to the queue.
   *
   * @param mixed $data 
   *   The data we wish to store.
   * @param mixed $unique_token 
   *   The unique token for the data. If NULL, the unique condition can be 
   *   ignored.
   * @param int $priority 
   *   A priority to store data as - items with higher values are retrieved over
   *   lower values.
   *
   * @return boolean 
   *   TRUE if the item was added to the DB, otherwise FALSE.
   */
  protected function enqueue($data, $unique_token = NULL, $priority = 0) {
    if (empty($priority)) { $priority = 0; }
    $record = (object) array(
      'queue_name' => $this->queue_name,
      'data' => serialize($data),
      'item_uuid' => $unique_token,
      'created' => time(),
      'priority' => $priority,
    );
    return drupal_write_record('unique_queue_items', $record) !== FALSE;
  }
  
  /**
   * Handles the occasional case where a queue item needs to be updated.
   * 
   * @param stdClass $queueItem 
   *   The queue item to update
   * 
   * @return boolean 
   *   The result of the write operation.
   */
  protected function updateQueueItem(stdClass $queueItem) {
    return drupal_write_record('unique_queue_items', $queueItem, array(
      'item_id'
    )) !== FALSE;
  }

  /**
   * Updates the priority of an existing item in the queue.
   *
   * @param string $unique_token 
   *   The hashed unique token.
   * @param int $priority 
   *   The new priority.
   *
   * @return boolean 
   *   TRUE if the item's priority was updated, otherwise FALSE.
   */
  protected function updatePriority($unique_token, $priority = 0) {
    if (empty($unique_token)) { return FALSE; }
    $results = db_update('unique_queue_items')
      ->condition('queue_name', $this->queue_name)
      ->condition('item_uuid', $unique_token)
      ->fields(array(
        'priority' => $priority,
      ))
      ->execute();
    return !empty($results);
  }

  /**
   * Retrieves the number of items left
   *
   * @return int 
   *   The number of items left in the queue
   */
  public function itemsLeft($min_priority = NULL) {
    $q = db_select('unique_queue_items');
    $q->condition('queue_name', $this->queue_name);
    $q->isNull('consumer_id');
    if ($min_priority !== NULL && $min_priority !== FALSE) {
      $q->condition('priority', $min_priority, '>=');
    }
    return $q->countQuery()->execute()->fetchField();
  }

  /**
   * Sets up a consumer ID.
   */
  protected function initConsumerId() {
    $this->consumer_id = db_insert('unique_queue_consumers')
        ->useDefaults(array('consumer_id'))
        ->execute();
  }

  /**
   * Retrieves the next queue item that could be locked.
   *
   * @param int $min_priority 
   *   The minimum priority to return. If NULL, priority will not be used as a 
   *   condition.
   *
   * @return stdClass 
   *   An object with data and item_id properties.
   */
  protected function peek($min_priority = NULL) {
    $q = db_select('unique_queue_items', 'uqi');
    $q->fields('uqi', array(
      'data',
      'item_id'
    ));
    $q->isNull('consumer_id');
    $q->isNull('lock_expires');
    $q->condition('queue_name', $this->queue_name);
    if ($min_priority !== NULL) {
      $q->condition('priority', $min_priority, '>=');
    }
    $q->orderBy('priority', 'DESC');
    $q->orderBy('created', 'ASC');
    // fallback sort for things created at the same time index.
    $q->orderBy('item_id', 'ASC');
    $q->range(0, 1);
    return $q->execute()->fetchObject();
  }

  /**
   * Establishes a lock on this item.
   *
   * @param stdClass $queue_object 
   *   The queue object as returned by claimItem().
   * @param int $lease_time 
   *   The length of time in seconds to lock the object for.
   *
   * @return boolean 
   *   TRUE if the object was locked, otherwise FALSE.
   */
  protected function establishLock(&$queue_object, $lease_time = 3600) {
    $q = db_update('unique_queue_items')
    ->fields(array(
      'consumer_id' => $this->consumer_id,
      'lock_expires' => time() + $lease_time,
    ))
    ->condition('item_id', $queue_object->item_id)
    ->isNull('consumer_id');

    if ($q->execute()) {
      $queue_object->data = unserialize($queue_object->data);
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Deletes an item from the queue, after it's done being processed.
   *
   * Note that this function may return FALSE if the object is no longer in the
   * database.
   *
   * @param stdClass $item 
   *   An object as returned by claimItem().
   *
   * @return boolean 
   *   TRUE if the object was deleted, otherwise FALSE.
   */
  public function deleteItem($item) {
    return !!db_delete('unique_queue_items')
      ->condition('item_id', $item->item_id)
      ->execute();
  }

  /**
   * Removes all items from this queue.
   *
   * @return boolean 
   *   TRUE if the objects were deleted, otherwise FALSE.
   */
  public function deleteQueue() {
    return !!db_delete('unique_queue_items')
      ->condition('queue_name', $this->queue_name)
      ->execute();
  }

  /**
   * Retrieves a list of all items currently in the queue, without locking them.
   *
   * Methods that use this to get queue items should NOT process the queue
   * items. This function exists for informational purposes only!
   *
   * @return array
   *   An iterable of queue objects.
   */
  public function listItems() {
    return db_select('unique_queue_items', 'uqi')
        ->fields('uqi', array(
          'item_uuid',
          'priority',
          'consumer_id',
          'lock_expires',
          'created',
          'item_id',
          'queue_name',
        ))
        ->condition('queue_name', $this->queue_name)
        ->orderBy('priority', 'DESC')
        ->orderBy('created', 'ASC')
        // fallback sort for things created at the same time index.
        ->orderBy('item_id', 'ASC')
        ->execute();
  }

  /**
   * Retrieves details on an item without locking it.
   *
   * Methods that use this should NOT process the queue item. This function
   * exists for informational purposes only!
   *
   * @param string $uuid
   *   The UUID to lookup
   *
   * @return stdClass 
   *   An object with all the details of a queue object:
   *   - item_uuid: The UUID of the item
   *   - priority: The numeric priority of the item
   *   - consumer_id: The current owner of the item, or empty if unowned
   *   - data: The unserialized data of the item
   *   - lock_expires: The time that the lock expires or empty if unlocked
   *   - created: The time the object was created
   */
  public function itemPeek($uuid) {
    return db_select('unique_queue_items', 'uqi')
        ->fields('uqi')
        ->condition('item_uuid', $uuid)
        ->condition('queue_name', $this->queue_name)
        ->execute()
        ->fetchObject();
  }

}