#ifndef _INCLUDE_FKZ_REPORTER_H_
#define _INCLUDE_FKZ_REPORTER_H_

// Periodic status updater.
// Posts the live server/player report to /servers/status and signals /servers/status/hibernate when the server empties.
void FKZ_SendReport();
void FKZ_SendHibernate();

#endif // _INCLUDE_FKZ_REPORTER_H_
