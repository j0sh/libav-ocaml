default: test

CFLAGS=-I"`ocamlc -where`" `pkg-config --cflags libavformat`
LIBS=`pkg-config --libs libavformat libswscale`

libav_stubs.o: libav_stubs.c
	gcc -g -c $(CFLAGS) $< $(LIBS)

libav.a : libav_stubs.o
	ocamlmklib -o libav $< $(LIBS)

libav.cmi:
	ocamlc libav.mli

libav.cmx: libav.cmi libav.a
	ocamlopt -g -c libav.ml

test.cmx:  libav.cmx
	ocamlopt -g -c $< test.ml

test: libav.cmx test.cmx
	ocamlopt -g -verbose -o $@ graphics.cmxa $^ -cclib "-L. -llibav $(LIBS)"

bbb: libav.cmx
	ocamlopt -g -verbose -o $@ graphics.cmxa $^ bbb.ml -cclib "-L. -llibav $(LIBS)"

main:
	ocamlfind ocamlc graphics.cma main.ml

clean:
	rm -f *.[oa] *.so *.cm[ixoa] *.cmxa test
