#pragma once

#include "roomer.h"

// Check if -q / --quit was requested. If so, sends QUIT command to running daemon and exits.
void daemon_handle_quit_flag(void);

// Try to send stdin image to running daemon.
// Returns true if sent successfully to daemon (caller can exit(0)).
// Returns false if daemon is not running (caller should run standalone).
bool daemon_client_send_image(void);

// Run the roomer daemon server loop.
// Pre-initializes hidden window, loads shaders/fonts/tablet, and listens on UNIX socket.
int daemon_server_run(void);
