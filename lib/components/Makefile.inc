noinst_LIBRARIES += components/libenigma_components.a

components_libenigma_components_a_SOURCES = \
	components/file_eraser.cpp \
	components/scan.cpp \
	components/tuxtxtapp.cpp \
	components/STBZone::GetInstance().cpp \
	components/socket_client.cpp

componentsincludedir = $(pkgincludedir)/lib/components
componentsinclude_HEADERS = \
	components/file_eraser.h \
	components/scan.h \
	components/STBZone::GetInstance().h \
	components/socket_client.h
