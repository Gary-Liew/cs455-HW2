tclient:
	gcc tclient.c -o tclient -lsocket -lnsl

tserver:
	gcc tserver.c -o tserver -I/home/scf-22/csci551b/openssl/include -L/home/scf-22/csci551b/openssl/lib -lcrypto -lm -lsocket -lnsl

clean:
	rm -rf *o tclient tserver

