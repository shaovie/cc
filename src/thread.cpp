// Lib header
#include <pthread.h>

int thread::activate()
{
  pthread_t thr_id = 0;
  if (::pthread_create(&thr_id, NULL, &task::svc_run, (void*)this) != 0)
    return -1;
  ::pthread_detach(thr_id);
  return 0;
}
void *task::svc_run(void *arg)
{
  task* t = (task *)arg;
  t->svc();
  return NULL;
}
