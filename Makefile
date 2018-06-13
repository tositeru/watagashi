CXX=clang++
CXX_FLAGS=-c -std=c++17 -O2 #-fno-rtti
#DEFINE_CLANG_PATH=-DBUILD_IN_CLANG_PATH=\"/usr/local/lib/clang/7.0.0/\"
LINK_LIBS=-lboost_system -lboost_filesystem -lboost_program_options -lpthread -lboost_stacktrace_basic
#LIBTOOLING_LIBS=-lclangTooling -lclangFrontendTool -lclangFrontend -lclangDriver -lclangSerialization -lclangCodeGen -lclangParse -lclangSema -lclangStaticAnalyzerFrontend -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore -lclangAnalysis -lclangARCMigrate -lclangRewrite -lclangRewriteFrontend -lclangEdit -lclangAST -lclangLex -lclangBasic -lclang
#LINK_OPTS=$(LIBTOOLING_LIBS) `llvm-config --ldflags` `llvm-config --libs all` `llvm-config --system-libs all`

OUTPUT_FILE=_watagashi
OBJS=main.o builder.o utility.o json11.o config.o errorReceiver.o programOptions.o includeFileAnalyzer.o specialVariables.o buildEnviroment.o processServer.o configJsonParser.o

_watagashi: $(OBJS)
	$(CXX) -o $(OUTPUT_FILE) $(OBJS) $(LINK_LIBS) #$(LINK_OPTS)

main.o: src/main.cpp
	$(CXX) -o main.o src/main.cpp $(CXX_FLAGS)
builder.o: src/builder.cpp
	$(CXX) -o builder.o src/builder.cpp $(CXX_FLAGS)
utility.o: src/utility.cpp
	$(CXX) -o utility.o src/utility.cpp $(CXX_FLAGS)
json11.o: src/json11/json11.cpp
	$(CXX) -o json11.o src/json11/json11.cpp $(CXX_FLAGS)
config.o: src/config.cpp
	$(CXX) -o config.o src/config.cpp $(CXX_FLAGS)
errorReceiver.o: src/errorReceiver.cpp
	$(CXX) -o errorReceiver.o src/errorReceiver.cpp $(CXX_FLAGS)
programOptions.o: src/programOptions.cpp
	$(CXX) -o programOptions.o src/programOptions.cpp $(CXX_FLAGS)
includeFileAnalyzer.o: src/includeFileAnalyzer.cpp
	$(CXX) -o includeFileAnalyzer.o src/includeFileAnalyzer.cpp $(CXX_FLAGS) #$(DEFINE_CLANG_PATH)
specialVariables.o: src/specialVariables.cpp
	$(CXX) -o specialVariables.o src/specialVariables.cpp $(CXX_FLAGS) #$(DEFINE_CLANG_PATH)
buildEnviroment.o: src/buildEnviroment.cpp
	$(CXX) -o buildEnviroment.o src/buildEnviroment.cpp $(CXX_FLAGS)
processServer.o: src/processServer.cpp
	$(CXX) -o processServer.o src/processServer.cpp $(CXX_FLAGS)
configJsonParser.o: src/configJsonParser.cpp
	$(CXX) -o configJsonParser.o src/configJsonParser.cpp $(CXX_FLAGS)

.PHONY: clean

clean:
	rm $(OUTPUT_FILE) *.o