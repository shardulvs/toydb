#PUBLICDIR= /usr0/cs564/public/project
SRC = buf.c hash.c pf.c
OBJ = buf.o hash.o pf.o
HDR = pftypes.h pf.h 

SPSRC = splayer.c
SPOBJ = splayer.o
SPHDR = splayer.h

pflayer.o: $(OBJ)
	ld -r -o pflayer.o $(OBJ)

tests: testhash testpf test_pf_experiments test_sp

testpf: testpf.o pflayer.o
	cc -o testpf testpf.o pflayer.o

testhash: testhash.o pflayer.o
	cc -o testhash testhash.o pflayer.o

test_pf_experiments: test_pf_experiments.o pflayer.o
	cc -o test_pf_experiments test_pf_experiments.o pflayer.o

test_sp: test_sp.o splayer.o pflayer.o
	cc -o test_sp test_sp.o splayer.o pflayer.o

$(OBJ): $(HDR)

splayer.o: splayer.c splayer.h $(HDR)
	cc -c splayer.c

test_sp.o: test_sp.c splayer.h $(HDR)
	cc -c test_sp.c

testhash.o: $(HDR)
testpf.o: $(HDR)
test_pf_experiments.o: $(HDR)

lint: 
	lint $(SRC)

install: pflayer.o 

clean:
	rm -f *.o \
	      testpf testhash test_pf_experiments test_sp \
	      file1 file2 \
	      pf_auto_testfile.dat pf_results.csv \
	      sp_student.dat sp_results.csv
