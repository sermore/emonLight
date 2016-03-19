#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/buzzer.o \
	${OBJECTDIR}/main.o \
	${OBJECTDIR}/receiver.o \
	${OBJECTDIR}/sender.o

# Test Directory
TESTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}/tests

# Test Files
TESTFILES= \
	${TESTDIR}/TestFiles/f1

# Test Object Files
TESTOBJECTFILES= \
	${TESTDIR}/tests/cunittest.o

# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/emonlight

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/emonlight: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/emonlight ${OBJECTFILES} ${LDLIBSOPTIONS} -lrt -lcurl -lwiringPi -lconfig

${OBJECTDIR}/buzzer.o: nbproject/Makefile-${CND_CONF}.mk buzzer.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/buzzer.o buzzer.c

${OBJECTDIR}/main.o: nbproject/Makefile-${CND_CONF}.mk main.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main.o main.c

${OBJECTDIR}/receiver.o: nbproject/Makefile-${CND_CONF}.mk receiver.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/receiver.o receiver.c

${OBJECTDIR}/sender.o: nbproject/Makefile-${CND_CONF}.mk sender.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/sender.o sender.c

# Subprojects
.build-subprojects:

# Build Test Targets
.build-tests-conf: .build-tests-subprojects .build-conf ${TESTFILES}
.build-tests-subprojects:

${TESTDIR}/TestFiles/f1: ${TESTDIR}/tests/cunittest.o ${OBJECTFILES:%.o=%_nomain.o}
	${MKDIR} -p ${TESTDIR}/TestFiles
	${LINK.c} -lrt -lcurl -lwiringPi -lconfig  -o ${TESTDIR}/TestFiles/f1 $^ ${LDLIBSOPTIONS} -lcunit 


${TESTDIR}/tests/cunittest.o: tests/cunittest.c 
	${MKDIR} -p ${TESTDIR}/tests
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -MMD -MP -MF "$@.d" -o ${TESTDIR}/tests/cunittest.o tests/cunittest.c


${OBJECTDIR}/buzzer_nomain.o: ${OBJECTDIR}/buzzer.o buzzer.c 
	${MKDIR} -p ${OBJECTDIR}
	@NMOUTPUT=`${NM} ${OBJECTDIR}/buzzer.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} "$@.d";\
	    $(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -Dmain=__nomain -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/buzzer_nomain.o buzzer.c;\
	else  \
	    ${CP} ${OBJECTDIR}/buzzer.o ${OBJECTDIR}/buzzer_nomain.o;\
	fi

${OBJECTDIR}/main_nomain.o: ${OBJECTDIR}/main.o main.c 
	${MKDIR} -p ${OBJECTDIR}
	@NMOUTPUT=`${NM} ${OBJECTDIR}/main.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} "$@.d";\
	    $(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -Dmain=__nomain -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main_nomain.o main.c;\
	else  \
	    ${CP} ${OBJECTDIR}/main.o ${OBJECTDIR}/main_nomain.o;\
	fi

${OBJECTDIR}/receiver_nomain.o: ${OBJECTDIR}/receiver.o receiver.c 
	${MKDIR} -p ${OBJECTDIR}
	@NMOUTPUT=`${NM} ${OBJECTDIR}/receiver.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} "$@.d";\
	    $(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -Dmain=__nomain -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/receiver_nomain.o receiver.c;\
	else  \
	    ${CP} ${OBJECTDIR}/receiver.o ${OBJECTDIR}/receiver_nomain.o;\
	fi

${OBJECTDIR}/sender_nomain.o: ${OBJECTDIR}/sender.o sender.c 
	${MKDIR} -p ${OBJECTDIR}
	@NMOUTPUT=`${NM} ${OBJECTDIR}/sender.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} "$@.d";\
	    $(COMPILE.c) -g -Wall -DVERSION=\"${GIT_VERSION}\" -I. -Dmain=__nomain -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/sender_nomain.o sender.c;\
	else  \
	    ${CP} ${OBJECTDIR}/sender.o ${OBJECTDIR}/sender_nomain.o;\
	fi

# Run Test Targets
.test-conf:
	@if [ "${TEST}" = "" ]; \
	then  \
	    ${TESTDIR}/TestFiles/f1 || true; \
	else  \
	    ./${TEST} || true; \
	fi

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/emonlight

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
