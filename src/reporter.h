#ifndef _INCLUDE_REPORTER_H_
#define _INCLUDE_REPORTER_H_

// Periodic status updater.
// Posts the live server/player report to /servers/status
// and signals /servers/status/hibernate when the server empties.
void SendReport();
void SendHibernate();

#endif // _INCLUDE_REPORTER_H_
