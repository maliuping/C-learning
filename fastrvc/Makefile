CONFIG_FILE := $(TARGET_PLATFORM)/config_file/fastrvc_config.xml
LOGO_FILE := $(TARGET_PLATFORM)/fastlogo_file/*

CC ?= gcc
RM ?= rm
CFLAGS += -g -Wall -I/usr/include/libdrm -I/usr/include/libxml2 -I/usr/include/cairo
LDLIBS += -pthread -ldrm -lxml2 -lcairo

TARGETINIT  := initforfastrvc
SOURCESINIT := \
	initforrvc.c \

OBJECTSINIT := $(SOURCESINIT:.c=.o)

SOURCESFASTRVC_COMMON := \
	xml.c \
	util.c \
	module.c \
	shm_buff.c \
	queue.c \
	module_file_input.c \
	module_file_output.c \
	module_v4l2.c \
	module_v4l2_mdp.c \
	module_v4l2_camera.c \
	module_ppm_logo.c \
	module_drm.c \
	drm_display.c \
	module_h264_logo.c \
	module_v4l2_list2va.c \
	module_v4l2_va2list.c \
	module_v4l2_codec.c \
	module_fourinone_camera.c \
	module_composite.c \
	module_v4l2_nr.c \
	module_fake.c \

TARGETFASTRVC  := fastrvc
SOURCESFASTRVC := $(SOURCESFASTRVC_COMMON) \
	fastrvc.c \

OBJECTSFASTRVC := $(SOURCESFASTRVC:.c=.o)

TARGETUT := rvc_ut
SOURCESUT := $(SOURCESFASTRVC_COMMON) \
	unit_test.c \

OBJECTSUT := $(SOURCESUT:.c=.o)

TARGETTOOL := rvc_tool
SOURCESTOOL := $(SOURCESFASTRVC_COMMON) \
	rvc_tool.c \

OBJECTSTOOL := $(SOURCESTOOL:.c=.o)

.PHONY: all clean install

all: $(TARGETFASTRVC) $(TARGETINIT) $(TARGETUT) $(TARGETTOOL)

$(TARGETFASTRVC): $(OBJECTSFASTRVC)
	$(CC) $^ $(CFLAGS) $(LDLIBS) -o $@
$(TARGETINIT): $(OBJECTSINIT)
	$(CC) $^ $(CFLAGS) -o $@
$(TARGETUT): $(OBJECTSUT)
	$(CC) $^ $(CFLAGS) $(LDLIBS) -o $@
$(TARGETTOOL): $(OBJECTSTOOL)
	$(CC) $^ $(CFLAGS) $(LDLIBS) -o $@

clean:
	$(RM) $(OBJECTSFASTRVC) $(TARGETFASTRVC) $(OBJECTS)
	$(RM) $(OBJECTSINIT) $(TARGETINIT) $(OBJECTS)
	$(RM) $(OBJECTSUT) $(TARGETUT) $(OBJECTS)
	$(RM) $(OBJECTSTOOL) $(TARGETTOOL) $(OBJECTS)

install:
	install -d $(DESTDIR)/bin/
	install -m 755 $(TARGETFASTRVC) $(DESTDIR)/bin/
	install -m 755 $(TARGETINIT) $(DESTDIR)/bin/
	install -m 755 $(TARGETUT) $(DESTDIR)/bin/
	install -m 755 $(TARGETTOOL) $(DESTDIR)/bin/
	install -m 644 $(CONFIG_FILE) $(DESTDIR)/bin/
	install -d $(DESTDIR)/bin/fastlogo_file
	install -m 644 $(LOGO_FILE) $(DESTDIR)/bin/fastlogo_file

-include $(patsubst %.o,%.d,$(OBJECTS))
