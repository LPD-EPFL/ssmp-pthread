#include "ssmp.h"

extern volatile ssmp_msg_t **ssmp_recv_buf;
extern volatile ssmp_msg_t **ssmp_send_buf;
extern ssmp_chunk_t **ssmp_chunk_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t *ssmp_barrier;

/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */


inline 
void ssmp_send(uint32_t to, volatile ssmp_msg_t *msg) 
{
  volatile ssmp_msg_t *tmpm = ssmp_send_buf[to];
  
#if defined(OPTERON) /* --------------------------------------- opteron */
#  ifdef USE_ATOMIC
  while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_EMPTY, BUF_LOCKD)) 
    {
      wait_cycles(WAIT_TIME);
    }
#  else 
  PREFETCHW(tmpm);
  while (tmpm->state != BUF_EMPTY)
    {
      _mm_pause();
      _mm_mfence();
      PREFETCHW(tmpm);
    }
#  endif

  msg->state = BUF_MESSG;
  memcpy((void*) tmpm, (const void*) msg, SSMP_CACHE_LINE_SIZE);

#elif defined(XEON) /* --------------------------------------- xeon */

  if (!ssmp_cores_on_same_socket(ssmp_id_, to))
    {
      while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_EMPTY, BUF_LOCKD)) 
	{
	  wait_cycles(WAIT_TIME);
	}
    }
  else
    {
      while (tmpm->state != BUF_EMPTY)
	{
	  _mm_pause();
	  _mm_mfence();
	}
    }
  msg->state = BUF_MESSG;
  memcpy((void*) tmpm, (const void*) msg, SSMP_CACHE_LINE_SIZE);
  _mm_mfence();

#elif defined(NIAGARA) /* --------------------------------------- niagara */
  while (tmpm->state != BUF_EMPTY)
    {
      _mm_pause();
    }

  tmpm->w0 = msg->w0;
  tmpm->w1 = msg->w1;
  tmpm->w2 = msg->w2;
  tmpm->state = BUF_MESSG;
#endif
}

  inline 
    void ssmp_send_big(int to, void *data, size_t length) 
  {
    int last_chunk = length % SSMP_CHUNK_SIZE;
    int num_chunks = length / SSMP_CHUNK_SIZE;

    while(num_chunks--) {

      while(ssmp_chunk_buf[ssmp_id_]->state);

      memcpy(ssmp_chunk_buf[ssmp_id_], data, SSMP_CHUNK_SIZE);
      data = ((char *) data) + SSMP_CHUNK_SIZE;

      ssmp_chunk_buf[ssmp_id_]->state = 1;
    }

    if (!last_chunk) {
      return;
    }

    while(ssmp_chunk_buf[ssmp_id_]->state);

    memcpy(ssmp_chunk_buf[ssmp_id_], data, last_chunk);

    ssmp_chunk_buf[ssmp_id_]->state = 1;

    PD("sent to %d", to);
  }
