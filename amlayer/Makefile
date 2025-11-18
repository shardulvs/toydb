a.out : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o ../pflayer/pflayer.o main.o amscan.o amprint.o
	cc am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o ../pflayer/pflayer.o main.o amscan.o amprint.o

amlayer.o : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o
	ld -r am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o -o amlayer.o

am.o : am.c am.h ../pflayer/pf.h ../pflayer/pftypes.h
	cc -c am.c

amfns.o : amfns.c am.h ../pflayer/pf.h ../pflayer/pftypes.h
	cc -c amfns.c

amsearch.o : amsearch.c am.h ../pflayer/pf.h ../pflayer/pftypes.h
	cc -c amsearch.c

aminsert.o : aminsert.c am.h ../pflayer/pf.h ../pflayer/pftypes.h
	cc -c aminsert.c

amscan.o : amscan.c am.h ../pflayer/pf.h ../pflayer/pftypes.h
	cc -c amscan.c

amstack.o : amstack.c am.h ../pflayer/pf.h ../pflayer/pftypes.h
	cc -c amstack.c

amglobals.o : amglobals.c am.h
	cc -c amglobals.c

amprint.o : amprint.c am.h ../pflayer/pf.h ../pflayer/pftypes.h
	cc -c amprint.c

main.o : main.c am.h ../pflayer/pf.h ../pflayer/pftypes.h
	cc -c main.c

OBJ = am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o

SPOBJ = ../pflayer/splayer.o

PFOBJ = ../pflayer/pflayer.o


build_from_file: build_from_file.o $(OBJ) $(PFOBJ) $(SPOBJ)
	cc -o build_from_file build_from_file.o $(OBJ) $(PFOBJ) $(SPOBJ)

build_from_file.o: build_from_file.c am.h
	cc -c build_from_file.c


build_incremental: build_incremental.o $(OBJ) $(PFOBJ) $(SPOBJ)
	cc -o build_incremental build_incremental.o $(OBJ) $(PFOBJ) $(SPOBJ)

build_incremental.o: build_incremental.c am.h
	cc -c build_incremental.c


bulk_load_index: bulk_load_index.o $(OBJ) $(PFOBJ) $(SPOBJ)
	cc -o bulk_load_index bulk_load_index.o $(OBJ) $(PFOBJ) $(SPOBJ)

bulk_load_index.o: bulk_load_index.c am.h
	cc -c bulk_load_index.c


test_queries: test_queries.o $(OBJ) $(PFOBJ) $(SPOBJ)
	cc -o test_queries test_queries.o $(OBJ) $(PFOBJ) $(SPOBJ)

test_queries.o: test_queries.c am.h
	cc -c test_queries.c


tests: a.out build_from_file build_incremental bulk_load_index test_queries



clean:
	rm -f *.o a.out build_from_file build_incremental bulk_load_index test_queries o[0-9]*
