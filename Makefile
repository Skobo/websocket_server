
#CC=arm-linux-gnueabihf-
CC=

target:
	$(CC)gcc -O3 -o websocket_client client_main.c websocket_common.c -lpthread
	$(CC)gcc -O3 -o websocket_server server_main.c config.c websocket_common.c cJSON.c -lpthread -lhiredis -lm

clean:
	@rm -rf client_process server_process
