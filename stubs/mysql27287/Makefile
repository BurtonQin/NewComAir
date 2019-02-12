# compile
CXX=clang++
OP_CFLAGS=-O0 -Xclang -disable-O0-optnone -g -flto
CFLAGS=-O0 -Xclang -disable-O0-optnone -g -flto
LDFLAGS=-use-gold-plugin -Wl,-plugin-opt=save-temps
TARGET=target
# lib path
PROJECT_DIR=/home/boqin/Projects/NewComAir/
LIB_DIR=${PROJECT_DIR}cmake-build-debug/
BIN_DIR=${PROJECT_DIR}bin/
INDVAR_FIND_PASS=${LIB_DIR}lib/LoopSampler/IndvarFinder/libIndvarFindPass.so
ID_PASS=${LIB_DIR}lib/IDAssigner/libIDAssignerPass.so
LOOP_INSTRUMENT_PASS=${LIB_DIR}lib/LoopSampler/LoopInstrumentor/libLoopInstrumentPass.so
RUNTIME_LIB=${LIB_DIR}runtime/libRuntimeLib.a
# bug fixed parts
INDVAR_INFO=indvar.pre.info
FUNC_NAME_ID=func_name_id.txt
TARGET=target.clonesample
ORIGIN_TARGET=target

# bug specific (mutable)
BUG_DIR=${PROJECT_DIR}stubs/mysql27287/
COMPILE_DIR=/home/boqin/Projects/ComAir_Bugs/mysql27287/buggy/build/sql/
SRC_FILE=${COMPILE_DIR}../../sql/item_xmlfunc.cc
BC_FILE=${BUG_DIR}mysqld.0.0.preopt.bc
LINENO=2518
FUNCNAME=_ZL14xml_parent_tagP14my_xml_node_stjj
# Sample once or twice consecutively
BELSEIF=
#BELSEIF=-bElseIf

# target and result
TARGET_DIR=${BUG_DIR}targets
RESULT_DIR=${BUG_DIR}results

.PHONY: all install clean

all: ${TARGET}${BELSEIF} ${ORIGIN_TARGET}

${ORIGIN_TARGET}: ${BC_FILE}
	${COMPILE_DIR}../libtool --preserve-dup-deps --tag=CXX --mode=link \
clang++  -flto -use-gold-plugin -Wl,-plugin-opt=save-temps -Xclang -disable-O0-optnone -g -O0 \
-DSAFE_MUTEX  -fno-implicit-templates -fno-exceptions -fno-rtti  -rdynamic \
-o $@ \
$< \
${COMPILE_DIR}../storage/csv/libcsv.a \
${COMPILE_DIR}../storage/heap/libheap.a \
${COMPILE_DIR}../storage/myisam/libmyisam.a \
${COMPILE_DIR}../storage/myisammrg/libmyisammrg.a \
${COMPILE_DIR}../vio/libvio.a \
${COMPILE_DIR}../mysys/libmysys.a \
${COMPILE_DIR}../dbug/libdbug.a \
${COMPILE_DIR}../regex/libregex.a \
${COMPILE_DIR}../strings/libmystrings.a \
-lz -ldl -lpthread -lcrypt -lnsl -lm -lrt

${TARGET}${BELSEIF}: ${TARGET}${BELSEIF}.lalls.bc ${RUNTIME_LIB}
	${COMPILE_DIR}../libtool --preserve-dup-deps --tag=CXX --mode=link \
clang++  -flto -use-gold-plugin -Wl,-plugin-opt=save-temps -Xclang -disable-O0-optnone -g -O0 \
-DSAFE_MUTEX  -fno-implicit-templates -fno-exceptions -fno-rtti  -rdynamic \
-o $@ \
$< \
${COMPILE_DIR}../storage/csv/libcsv.a \
${COMPILE_DIR}../storage/heap/libheap.a \
${COMPILE_DIR}../storage/myisam/libmyisam.a \
${COMPILE_DIR}../storage/myisammrg/libmyisammrg.a \
${COMPILE_DIR}../vio/libvio.a \
${COMPILE_DIR}../mysys/libmysys.a \
${COMPILE_DIR}../dbug/libdbug.a \
${COMPILE_DIR}../regex/libregex.a \
${COMPILE_DIR}../strings/libmystrings.a \
-lz -ldl -lpthread -lcrypt -lnsl -lm -lrt -l${RUNTIME_LIB}

${TARGET}${BELSEIF}.lalls.bc: ${TARGET}.bc.id ${LOOP_INSTRUMENT_PASS} ${SRC_FILE} ${INDVAR_INFO}
	opt -load ${LOOP_INSTRUMENT_PASS} -loop-instrument \
	 -noLine ${LINENO} -strFile ${SRC_FILE} \
	 -strFunc ${FUNCNAME} ${BELSEIF} $< > $@

${INDVAR_INFO}: ${TARGET}.m2r.bc ${INDVAR_FIND_PASS}
	opt -load ${INDVAR_FIND_PASS} -indvar-find -noLine ${LINENO} -strFile ${SRC_FILE} -strFunc ${FUNCNAME} $< > /dev/null

${TARGET}.m2r.bc: ${TARGET}.bc.id
	opt -mem2reg $< > $@

${TARGET}.bc.id: ${TARGET}.ls.bc ${ID_PASS}
	opt -load ${ID_PASS} -tag-id $< > $@

${TARGET}.ls.bc: ${BC_FILE}
	opt -loop-simplify $< > $@

install: ${TARGET}${BELSEIF} ${ORIGIN_TARGET} ${FUNC_NAME_ID} ${TARGET_DIR} ${RESULT_DIR}
	cp ${TARGET}${BELSEIF} ${ORIGIN_TARGET} ${TARGET_DIR}
	cp ${FUNC_NAME_ID} ${RESULT_DIR} 
	cp indvar.info ${RESULT_DIR}

${TARGET_DIR}:
	mkdir -p $@

${RESULT_DIR}:
	mkdir -p $@

clean:
	rm -rf *.o ${TARGET}.* *.bc *.resolution.txt *.id target.aprof *.ll *.opt *.bb *.txt

