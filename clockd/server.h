#ifndef SERVER_H
#define SERVER_H

/**
 * @brief Prototypes for clockd 'server'
 * @file  server.h
 * @copyright GNU GPLv2 or later
 */


/**
 * Init D-Bus server
 *
 * The following is set up:
 *
 * - settings from environment (/etc/clockd/clockd-settings.default)
 * - settings from configuration file (/home/user/.clockd.conf)
 * - D-Bus connections
 * - libosso init
 * - timezone from /etc/localtime or /home/user/.clockd.conf
 *
 * @return  0 if OK, -1 if error
 */
int server_init(void);

/**
 * Deinitialize D-Bus server
 */
void server_quit(void);

#endif  /* SERVER_H */
