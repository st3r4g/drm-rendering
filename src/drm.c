#include <mydrm.h>

#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <stdio.h> // for fprintf
#include <stdint.h> // for uint_64
#include <stdlib.h>

#include <errno.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

const char *conn_get_name(uint32_t type_id) {
	switch (type_id) {
	case DRM_MODE_CONNECTOR_Unknown:     return "Unknown";
	case DRM_MODE_CONNECTOR_VGA:         return "VGA";
	case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
	case DRM_MODE_CONNECTOR_Composite:   return "Composite";
	case DRM_MODE_CONNECTOR_SVIDEO:      return "SVIDEO";
	case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
	case DRM_MODE_CONNECTOR_Component:   return "Component";
	case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
	case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
	case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
	case DRM_MODE_CONNECTOR_TV:          return "TV";
	case DRM_MODE_CONNECTOR_eDP:         return "eDP";
	case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
	case DRM_MODE_CONNECTOR_DSI:         return "DSI";
	default:                             return "Unknown";
	}
}

const char *conn_get_connection(drmModeConnection connection) {
	switch (connection) {
	case DRM_MODE_CONNECTED:         return "connected";
	case DRM_MODE_DISCONNECTED:      return "disconnected";
	case DRM_MODE_UNKNOWNCONNECTION: return "unknown";
	default:                         return "unknown";
	}
}

int display_info(const char *card)
{
	int ret, fd;
	drmModeRes *res;
	int n_conn, n_crtc;

	fd = open(card, O_RDWR);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	printf("\ncard: %s\n", card);
	
	res = drmModeGetResources(fd);
	if (res == NULL) {
		fprintf(stderr, "drmModeGetResources failed");
		close(fd);
		return -1;
	}
	
	n_conn = res->count_connectors;
	n_crtc = res->count_crtcs;
	printf("n_conn = %i, n_crtc = %i\n", n_conn, n_crtc);

	for (int i=0; i < n_conn; i++) {
		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		printf("type: %s, status: %s\n", conn_get_name(conn->connector_type),
		conn_get_connection(conn->connection));
		drmModeFreeConnector(conn);
	}

	drmModeFreeResources(res);
	ret = close(fd);
	if (ret < 0) {
		perror("close");
		return -1;
	}

	return 0;
}
