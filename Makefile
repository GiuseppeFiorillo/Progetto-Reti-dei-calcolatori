# compilatore utilizzato
CC = gcc
# abilita tutti i warning del compilatore
CFLAGS = -g -Wall

all : clean client clientT clientS centro_vaccinale serverV serverG

client : client.c green_pass.h addresses.h
	$(CC) $(CFLAGS) client.c -o client

clientT : clientT.c green_pass.h addresses.h
	$(CC) $(CFLAGS) clientT.c -o clientT

clientS : clientS.c green_pass.h addresses.h
	$(CC) $(CFLAGS) clientS.c -o clientS

centro_vaccinale : centro_vaccinale.c green_pass.h addresses.h
	$(CC) $(CFLAGS) centro_vaccinale.c -o centro_vaccinale

serverV : serverV.c green_pass.h addresses.h
	$(CC) $(CFLAGS) serverV.c -o serverV

serverG : serverG.c green_pass.h addresses.h
	$(CC) $(CFLAGS) serverG.c -o serverG

clean :
	rm -f *.o all
	rm -f clientT
	rm -f clientS
	rm -f client
	rm -f centro_vaccinale
	rm -f serverG
	rm -f serverV

