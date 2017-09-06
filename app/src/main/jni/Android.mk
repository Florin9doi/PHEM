LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LOCAL_PATH)/SrcShared/jpeg/jcapimin.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jcmarker.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jcomapi.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdapimin.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdapistd.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdcoefct.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdcolor.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jddctmgr.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdhuff.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdinput.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdmainct.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdmarker.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdmaster.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdmerge.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdphuff.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdpostct.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jdsample.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jidctflt.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jidctfst.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jidctint.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jidctred.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jmemmac.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jmemmgr.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jquant1.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jquant2.c \
  $(LOCAL_PATH)/SrcShared/jpeg/jutils.c \


LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/SrcAndroid/ \
  $(LOCAL_PATH)/SrcShared/ \
  $(LOCAL_PATH)/SrcShared/jpeg/ \

LOCAL_MODULE := poserjpeg

include $(BUILD_STATIC_LIBRARY)

##################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  $(LOCAL_PATH)/SrcShared/ATraps.cpp \
  $(LOCAL_PATH)/SrcShared/Byteswapping.cpp \
  $(LOCAL_PATH)/SrcShared/CGremlins.cpp \
  $(LOCAL_PATH)/SrcShared/CGremlinsStubs.cpp \
  $(LOCAL_PATH)/SrcShared/ChunkFile.cpp \
  $(LOCAL_PATH)/SrcShared/DebugMgr.cpp \
  $(LOCAL_PATH)/SrcShared/EmAction.cpp \
  $(LOCAL_PATH)/SrcShared/EmApplication.cpp \
  $(LOCAL_PATH)/SrcShared/EmCommon.cpp \
  $(LOCAL_PATH)/SrcShared/EmDevice.cpp \
  $(LOCAL_PATH)/SrcShared/EmDirRef.cpp \
  $(LOCAL_PATH)/SrcShared/EmDlg.cpp \
  $(LOCAL_PATH)/SrcShared/EmDocument.cpp \
  $(LOCAL_PATH)/SrcShared/EmEventOutput.cpp \
  $(LOCAL_PATH)/SrcShared/EmEventPlayback.cpp \
  $(LOCAL_PATH)/SrcShared/EmException.cpp \
  $(LOCAL_PATH)/SrcShared/EmExgMgr.cpp \
  $(LOCAL_PATH)/SrcShared/EmFileImport.cpp \
  $(LOCAL_PATH)/SrcShared/EmFileRef.cpp \
  $(LOCAL_PATH)/SrcShared/EmJPEG.cpp \
  $(LOCAL_PATH)/SrcShared/EmLowMem.cpp \
  $(LOCAL_PATH)/SrcShared/EmMapFile.cpp \
  $(LOCAL_PATH)/SrcShared/EmMenus.cpp \
  $(LOCAL_PATH)/SrcShared/EmMinimize.cpp \
  $(LOCAL_PATH)/SrcShared/EmPalmFunction.cpp \
  $(LOCAL_PATH)/SrcShared/EmPalmHeap.cpp \
  $(LOCAL_PATH)/SrcShared/EmPalmOS.cpp \
  $(LOCAL_PATH)/SrcShared/EmPalmStructs.cpp \
  $(LOCAL_PATH)/SrcShared/EmPixMap.cpp \
  $(LOCAL_PATH)/SrcShared/EmPoint.cpp \
  $(LOCAL_PATH)/SrcShared/EmQuantizer.cpp \
  $(LOCAL_PATH)/SrcShared/EmRect.cpp \
  $(LOCAL_PATH)/SrcShared/EmRefCounted.cpp \
  $(LOCAL_PATH)/SrcShared/EmRegion.cpp \
  $(LOCAL_PATH)/SrcShared/EmROMReader.cpp \
  $(LOCAL_PATH)/SrcShared/EmROMTransfer.cpp \
  $(LOCAL_PATH)/SrcShared/EmRPC.cpp \
  $(LOCAL_PATH)/SrcShared/EmScreen.cpp \
  $(LOCAL_PATH)/SrcShared/EmSession.cpp \
  $(LOCAL_PATH)/SrcShared/EmStream.cpp \
  $(LOCAL_PATH)/SrcShared/EmStreamFile.cpp \
  $(LOCAL_PATH)/SrcShared/EmSubroutine.cpp \
  $(LOCAL_PATH)/SrcShared/EmThreadSafeQueue.cpp \
  $(LOCAL_PATH)/SrcShared/EmTransport.cpp \
  $(LOCAL_PATH)/SrcShared/EmTransportSerial.cpp \
  $(LOCAL_PATH)/SrcShared/EmTransportSocket.cpp \
  $(LOCAL_PATH)/SrcShared/EmTransportUSB.cpp \
  $(LOCAL_PATH)/SrcShared/EmWindow.cpp \
  $(LOCAL_PATH)/SrcShared/ErrorHandling.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmBankDRAM.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmBankDummy.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmBankMapped.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmBankRegs.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmBankROM.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmBankSRAM.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmCPU68K.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmCPUARM.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmCPU.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmHAL.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmMemory.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegs328.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegs328PalmPilot.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegs328Symbol1700.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsASICSymbol1700.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegs.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZ.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZPalmIIIc.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZPalmM100.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZPalmV.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZPalmVII.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZPalmVIIx.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZTemp.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZTRGpro.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsEZVisor.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsFrameBuffer.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsMediaQ11xx.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsPLDPalmVIIEZ.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsSED1375.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsSED1376.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsSZ.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsSZTemp.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsUSBPhilipsPDIUSBD12.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsUSBVisor.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsVZ.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsVZHandEra330.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsVZPalmM500.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsVZPalmM505.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsVZTemp.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsVZVisorEdge.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsVZVisorPlatinum.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmRegsVZVisorPrism.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmSPISlaveADS784x.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmSPISlave.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmUAEGlue.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/EmUARTDragonball.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmRegs330CPLD.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmSPISlave330Current.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmTRGATA.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmTRGCF.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmTRGCFIO.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmTRGCFMem.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmTRG.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmTRGDiskIO.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmTRGDiskType.cpp \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/EmTRGSD.cpp \
  $(LOCAL_PATH)/SrcShared/Hordes.cpp \
  $(LOCAL_PATH)/SrcShared/HostControl.cpp \
  $(LOCAL_PATH)/SrcShared/LoadApplication.cpp \
  $(LOCAL_PATH)/SrcShared/Logging.cpp \
  $(LOCAL_PATH)/SrcShared/Marshal.cpp \
  $(LOCAL_PATH)/SrcShared/MetaMemory.cpp \
  $(LOCAL_PATH)/SrcShared/Miscellaneous.cpp \
  $(LOCAL_PATH)/SrcShared/omnithread/posix.cpp \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Core/System/Src/Crc.c \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchLoader.cpp \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchMgr.cpp \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchModule.cpp \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchModuleHtal.cpp \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchModuleMap.cpp \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchModuleMemMgr.cpp \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchModuleNetLib.cpp \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchModuleSys.cpp \
  $(LOCAL_PATH)/SrcShared/Patches/EmPatchState.cpp \
  $(LOCAL_PATH)/SrcShared/Platform_NetLib_Sck.cpp \
  $(LOCAL_PATH)/SrcShared/PreferenceMgr.cpp \
  $(LOCAL_PATH)/SrcShared/Profiling.cpp \
  $(LOCAL_PATH)/SrcShared/ROMStubs.cpp \
  $(LOCAL_PATH)/SrcShared/SessionFile.cpp \
  $(LOCAL_PATH)/SrcShared/Skins.cpp \
  $(LOCAL_PATH)/SrcShared/SLP.cpp \
  $(LOCAL_PATH)/SrcShared/SocketMessaging.cpp \
  $(LOCAL_PATH)/SrcShared/Startup.cpp \
  $(LOCAL_PATH)/SrcShared/StringConversions.cpp \
  $(LOCAL_PATH)/SrcShared/StringData.cpp \
  $(LOCAL_PATH)/SrcShared/SystemPacket.cpp \
  $(LOCAL_PATH)/SrcShared/TracerCommon.cpp \
  \
  $(LOCAL_PATH)/SrcShared/UAE/cpudefs.c \
  $(LOCAL_PATH)/SrcShared/UAE/cpuemu.c \
  $(LOCAL_PATH)/SrcShared/UAE/cpustbl.c \
  $(LOCAL_PATH)/SrcShared/UAE/readcpu.cpp \
  \
  $(LOCAL_PATH)/SrcAndroid/EmTransportSerialAndroid.cpp \
  $(LOCAL_PATH)/SrcAndroid/EmPixMapAndroid.cpp \
  $(LOCAL_PATH)/SrcAndroid/EmWindowAndroid.cpp \
  $(LOCAL_PATH)/SrcAndroid/PHEMNativeIF.cpp \
  $(LOCAL_PATH)/SrcAndroid/EmDocumentAndroid.cpp \
  $(LOCAL_PATH)/SrcAndroid/EmTransportUSBAndroid.cpp \
  $(LOCAL_PATH)/SrcAndroid/Platform_Android.cpp \
  $(LOCAL_PATH)/SrcAndroid/EmFileRefAndroid.cpp \
  $(LOCAL_PATH)/SrcAndroid/EmDirRefAndroid.cpp \
  $(LOCAL_PATH)/SrcAndroid/EmApplicationAndroid.cpp \
  $(LOCAL_PATH)/SrcAndroid/EmDlgAndroid.cpp \
  \
  $(LOCAL_PATH)/SrcShared/Gzip/util.c \
  $(LOCAL_PATH)/SrcShared/Gzip/inflate.c \
  $(LOCAL_PATH)/SrcShared/Gzip/bits.c \
  $(LOCAL_PATH)/SrcShared/Gzip/trees.c \
  $(LOCAL_PATH)/SrcShared/Gzip/deflate.c \
  \
  $(LOCAL_PATH)/SrcShared/ResStrings.cpp \


LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/SrcAndroid/ \
  $(LOCAL_PATH)/SrcShared/ \
  $(LOCAL_PATH)/SrcShared/Gzip/ \
  $(LOCAL_PATH)/SrcShared/Hardware/ \
  $(LOCAL_PATH)/SrcShared/Hardware/TRG/ \
  $(LOCAL_PATH)/SrcShared/jpeg/ \
  $(LOCAL_PATH)/SrcShared/omnithread/ \
  $(LOCAL_PATH)/SrcShared/Palm/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/328Jerry/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/328Jerry/IncsPrv/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/EZAustin/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/EZAustin/IncsPrv/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/EZSumo/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/EZSumo/IncsPrv/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/VZTrn/ \
  $(LOCAL_PATH)/SrcShared/Palm/Device/VZTrn/IncsPrv/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Core/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Core/Hardware/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Core/Hardware/IncsPrv/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Core/System/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Core/System/IncsPrv/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Core/System/Src/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Incs/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Incs/Core/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Incs/Core/Hardware/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Incs/Core/System/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Incs/Core/UI/ \
  $(LOCAL_PATH)/SrcShared/Palm/Platform/Incs/Libraries/ \
  $(LOCAL_PATH)/SrcShared/Patches/ \
  $(LOCAL_PATH)/SrcShared/UAE/ \


LOCAL_CFLAGS := \
  -lpthread \
  -frtti -fexceptions -Wno-multichar -Wno-unknown-pragmas -Wno-conversion \
  -DPthreadDraftVersion=10 \
  -DPLATFORM_UNIX=1 -D__PALMOS_TRAPS__=0 -DEMULATION_LEVEL=EMULATION_UNIX -O2 -DHAS_PROFILING=0 -DNDEBUG \
  -DHAVE_DIRENT_H=1 -DHAVE_ENDIAN_H=1 -DHAVE_TYPE_SOCKLEN_T=1 \

LOCAL_LDFLAGS := -llog

LOCAL_STATIC_LIBRARIES := poserjpeg

LOCAL_MODULE := pose

include $(BUILD_SHARED_LIBRARY)
