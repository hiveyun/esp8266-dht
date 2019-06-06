all: dht_fsm dht_fsm.pdf

dht_fsm: dht_fsm.smudge
	smudge $<

dht_fsm.pdf: dht_fsm.gv
	dot -Tpdf dht_fsm.gv > $@

dht_fsm.gv: dht_fsm

clean:
	rm -f dht_fsm.c
	rm -f dht_fsm.h
	rm -f dht_fsm.pdf
	rm -f dht_fsm_ext.h
	rm -f dht_fsm.gv
