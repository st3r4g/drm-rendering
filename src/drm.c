#include <mydrm.h>

#include <inttypes.h>
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

int display_info(int fd)
{
	drmModeRes *res;
	int n_conn, n_enc, n_crtc;
	drmModeConnector **Connector_l;
//	drmModeEncoder **Encoder_l;
	drmModeCrtc **Crtc_l;

	res = drmModeGetResources(fd);
	if (res == NULL) {
		fprintf(stderr, "drmModeGetResources failed");
		return -1;
	}
	
	n_conn = res->count_connectors;
	n_enc = res->count_encoders;
	n_crtc = res->count_crtcs;

	printf("n_conn = %i, n_enc = %i, n_crtc = %i\n", n_conn, n_enc, n_crtc);

	Connector_l = calloc(n_conn, sizeof(drmModeConnector*));
//	Encoder_l = calloc(n_enc, sizeof(drmModeEncoder*));
	Crtc_l = calloc(n_crtc, sizeof(drmModeCrtc*));
	
	printf("Scanning connectors:\n");
	for (int i=0; i<n_conn; ++i) {
		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		Connector_l[i] = conn;
		printf("type: %s, status: %s, n_enc: %i %"PRIu32" %"PRIu32"\n",
		conn_get_name(conn->connector_type),
		conn_get_connection(conn->connection), conn->count_encoders,
		conn->encoder_id, conn->encoders[0]);
		for (int j=0; j<conn->count_modes; ++j) {
			printf("	%s\n", conn->modes[i].name);
		}
	}

	printf("Scanning crtcs:\n");
	for (int i=0; i<n_crtc; ++i) {
		drmModeCrtc *crtc = drmModeGetCrtc(fd, res->crtcs[i]);
		Crtc_l[i] = crtc;
		printf("mode: %s valid: %i\n", crtc->mode.name, crtc->mode_valid);
	}

//	for (int i=0; i<n_enc; ++i) {
//		Encoder_l[i] = drmModeGetEncoder(fd, res->connectors[i]);
//	}


	for (int i=0; i<n_conn; ++i) {
		drmModeFreeConnector(Connector_l[i]);
	}
	free(Connector_l);
	for (int i=0; i<n_crtc; ++i) {
		drmModeFreeCrtc(Crtc_l[i]);
	}
	free(Crtc_l);

	drmModeFreeResources(res);

	return 0;
}
