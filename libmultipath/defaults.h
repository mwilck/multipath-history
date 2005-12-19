#define DEFAULT_GETUID		"/sbin/scsi_id -g -u -s /block/%n"
#define DEFAULT_UDEVDIR		"/dev"
#define DEFAULT_SELECTOR	"round-robin 0"
#define DEFAULT_FEATURES	"0"
#define DEFAULT_HWHANDLER	"0"
#define DEFAULT_MINIO		1000

#define DEFAULT_TARGET		"multipath"
#define DEFAULT_PIDFILE		"/var/run/multipathd.pid"
#define DEFAULT_SOCKET		"/var/run/multipathd.sock"
#define DEFAULT_CONFIGFILE	"/etc/multipath.conf"
#define DEFAULT_BINDINGS_FILE  "/var/lib/multipath/bindings"

char * set_default (char * str);
