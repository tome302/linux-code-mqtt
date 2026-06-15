# Makefile for OneNET MQTT Client

# ======================== 编译器配置 ========================

CC = gcc
CFLAGS = -Wall -O2 -I./include

# 本地 mosquitto 2.0.20 动态库
MOSQ_LIB_DIR = ./lib
MOSQ_RPATH = \$$ORIGIN/lib

LDFLAGS = -L$(MOSQ_LIB_DIR) -lmosquitto -ljson-c -lpthread \
          -Wl,-rpath,$(MOSQ_RPATH)

# ======================== 目标定义 ========================

TARGET = onenet_mqtt_client
SRCS = src/main.c \
       src/onenet_mqtt_client.c \
       src/onenet_property.c

OBJS = $(SRCS:.c=.o)

# ======================== 编译规则 ========================

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)
	@echo "Build completed: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "  Compiling: $<"

# ======================== 清理规则 ========================

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Clean completed"

# ======================== 调试版本 ========================

debug: CFLAGS += -g -DDEBUG
debug: clean all
	@echo "Debug build completed"

# ======================== 依赖关系 ========================

src/main.o: src/main.c include/onenet_config.h include/onenet_mqtt.h include/onenet_property.h include/common_type.h
src/onenet_mqtt_client.o: src/onenet_mqtt_client.c include/onenet_config.h include/onenet_mqtt.h include/common_type.h include/mosquitto.h
src/onenet_property.o: src/onenet_property.c include/onenet_property.h include/common_type.h

.PHONY: all clean debug
