#include <stdlib.h>
#include "wq.h"
#include "utlist.h"

/* Initializes a work queue WQ. */
void wq_init(wq_t *wq) {

  /* TODO: Make me thread-safe! */

  wq->size = 0;
  wq->head = NULL;

  // Initialize our (custom) synchronization fields:
  pthread_cond_init(&wq->cond_var, NULL);
  pthread_mutex_init(&wq->guard, NULL);
}

/* Remove an item from the WQ. This function should block until there
 * is at least one item on the queue. */
int wq_pop(wq_t *wq) {

  /* TODO: Make me blocking and thread-safe! */

  // before we can make changes, we need to acquire the lock so at most one
  // thread is modifying wq.
  pthread_mutex_lock(&wq->guard);

  // We now have exlusive access to wq. Our intention is to pop an element
  // from wq->head. However, before we do so, we have to make sure that there
  // are avaialable jobs to begin with. Thus we will block until the condition
  // 'wq->size > 0' becomes true.
  // ie, block while 'wq->size < 1' is true.
  while (wq->size < 1) {
    // working queue is empty so sleep until someone adds element to it.
    // make sure to release the lock so other can work on wq while i sleep.
    // pthread_cond_wait() ensures that this happens.
    pthread_cond_wait(&wq->cond_var, &wq->guard);
  }
  // now we have at least one job in the queue. lets remove it from the queue.
  wq_item_t *wq_item = wq->head;
  int client_socket_fd = wq->head->client_socket_fd;
  wq->size--;
  DL_DELETE(wq->head, wq->head);
  free(wq_item);
  // done modifying wq. lets release the lock.
  pthread_mutex_unlock(&wq->guard);
  return client_socket_fd;
}

/* Add ITEM to WQ. */
void wq_push(wq_t *wq, int client_socket_fd) {

  /* TODO: Make me thread-safe! */
  /*
      lets make sure that at most one thread is able to make chages to wq->head
      or wq->size.
      which can happen via wq_push() or wq_pop().
  */
  // grab the lock. else go to sleep until woken up.
  // remember the '->' operator has precedence over the '&' operator.
  pthread_mutex_lock(&wq->guard);

  // now we are the only thread that can make changes to wq.
  wq_item_t *wq_item = calloc(1, sizeof(wq_item_t));
  wq_item->client_socket_fd = client_socket_fd;
  DL_APPEND(wq->head, wq_item);
  wq->size++;

  // now in case there are blocked threads (waiters) waiting for the working
  // queue to  have an available jobs (clients sockets who need to be served),
  // we will signal one of them.
  // I.e., these are threads who are waiting for the condition 'wq->size > 0'
  // to become true.
  pthread_cond_signal(&wq->cond_var);

  // finally, release the lock, so that others can make acquire it and make
  // changes to wq.
  pthread_mutex_unlock(&wq->guard);
}
