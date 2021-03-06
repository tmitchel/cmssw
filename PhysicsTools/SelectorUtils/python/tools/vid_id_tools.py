import FWCore.ParameterSet.Config as cms

from PhysicsTools.SelectorUtils.centralIDRegistry import central_id_registry

import sys

#keep python2.6 compatibility for computing
#import importlib

#general simple tools for various object types
def setupVIDSelection(vidproducer,cutflow):
    if not hasattr(cutflow,'idName'):
        raise Exception('InvalidVIDCutFlow', 'The cutflow configuation provided is malformed and does not have a specified name!')
    if not hasattr(cutflow,'cutFlow'):
        raise Exception('InvalidVIDCutFlow', 'The cutflow configuration provided is malformed and does not have a specific cutflow!')
    cutflow_md5 = central_id_registry.getMD5FromName(cutflow.idName)
    isPOGApproved = False
    if hasattr(cutflow,'isPOGApproved'):
        isPOGApproved = cutflow.isPOGApproved.value()
    vidproducer.physicsObjectIDs.append(
        cms.PSet( idDefinition = cutflow,
                  isPOGApproved = cms.untracked.bool(isPOGApproved),
                  idMD5 = cms.string(cutflow_md5) )
    )    
#    sys.stderr.write('Added ID \'%s\' to %s\n'%(cutflow.idName.value(),vidproducer.label()))

def addVIDSelectionToPATProducer(patProducer,idProducer,idName,addUserData=True):
    patProducerIDs = None
    userDatas = None
    for key in patProducer.__dict__.keys():
        if 'IDSources' in key:
            patProducerIDs = getattr(patProducer,key)
        if 'userData' in key:
            userDatas = getattr(patProducer,key)
    if patProducerIDs is None:
        raise Exception('StrangePatModule','%s does not have ID sources!'%patProducer.label())
    if userDatas is None:
        raise Exception('StrangePatModule','%s does not have UserData sources!'%patProducer.label())
    setattr(patProducerIDs,idName,cms.InputTag('%s:%s'%(idProducer,idName)))    
    if( addUserData ):
        if( len(userDatas.userClasses.src) == 1 and 
            type(userDatas.userClasses.src[0]) is str and 
            userDatas.userClasses.src[0] == ''            ):
            userDatas.userClasses.src = cms.VInputTag(cms.InputTag('%s:%s'%(idProducer,idName)))        
        else:
            userDatas.userClasses.src.append(cms.InputTag('%s:%s'%(idProducer,idName)))
        sys.stderr.write('\t--- %s:%s added to %s\n'%(idProducer,idName,patProducer.label()))

def setupAllVIDIdsInModule(process,id_module_name,setupFunction,patProducer=None,addUserData=True,task=None):
#    idmod = importlib.import_module(id_module_name)
    idmod= __import__(id_module_name, globals(), locals(), ['idName','cutFlow'])
    for name in dir(idmod):
        item = getattr(idmod,name)
        if hasattr(item,'idName') and hasattr(item,'cutFlow'):
            setupFunction(process,item,patProducer,addUserData,task)

# Supported data formats defined via "enum"
from PhysicsTools.SelectorUtils.tools.DataFormat import DataFormat

####
# Electrons
####

#turns on the VID electron ID producer, possibly with extra options
# for PAT and/or MINIAOD
def switchOnVIDElectronIdProducer(process, dataFormat, task=None):
    process.load('RecoEgamma.ElectronIdentification.egmGsfElectronIDs_cff')
    if task is not None:
        task.add(process.egmGsfElectronIDTask)
    #*always* reset to an empty configuration
    if( len(process.egmGsfElectronIDs.physicsObjectIDs) > 0 ):
        process.egmGsfElectronIDs.physicsObjectIDs = cms.VPSet()
    dataFormatString = "Undefined"
    if dataFormat == DataFormat.AOD:
        # No reconfiguration is required, default settings are for AOD
        dataFormatString = "AOD"
    elif dataFormat == DataFormat.MiniAOD:
        # If we are dealing with MiniAOD, we overwrite the electron collection
        # name appropriately, for the fragment we just loaded above. 
        process.egmGsfElectronIDs.physicsObjectSrc = cms.InputTag('slimmedElectrons')
        dataFormatString = "MiniAOD"
    else:
        raise Exception('InvalidVIDDataFormat', 'The requested data format is different from AOD or MiniAOD')
    #    
#    sys.stderr.write('Added \'egmGsfElectronIDs\' to process definition (%s format)!\n' % dataFormatString)

def setupVIDElectronSelection(process,cutflow,patProducer=None,addUserData=True,task=None):
    if not hasattr(process,'egmGsfElectronIDs'):
        raise Exception('VIDProducerNotAvailable','egmGsfElectronIDs producer not available in process!')
    setupVIDSelection(process.egmGsfElectronIDs,cutflow)
    #add to PAT electron producer if available or specified
    if hasattr(process,'patElectrons') or patProducer is not None:
        if patProducer is None:
            patProducer = process.patElectrons
        idName = cutflow.idName.value()
        addVIDSelectionToPATProducer(patProducer,'egmGsfElectronIDs',idName,addUserData)

    #we know cutflow has a name otherwise an exception would have been thrown in setupVIDSelection
    #run this for all heep IDs except V60 standard for which it is not needed
    #it is needed for V61 and V70
    if (cutflow.idName.value().find("HEEP")!=-1 and
        cutflow.idName.value()!="heepElectronID-HEEPV60"):

        #not the ideal way but currently the easiest
        useMiniAOD = process.egmGsfElectronIDs.physicsObjectSrc == cms.InputTag('slimmedElectrons')
            
        from RecoEgamma.ElectronIdentification.Identification.heepElectronID_tools import addHEEPProducersToSeq
        addHEEPProducersToSeq(process=process,seq=process.egmGsfElectronIDSequence,
                              useMiniAOD=useMiniAOD,task=task)
        
####
# Muons
####

#turns on the VID electron ID producer, possibly with extra options
# for PAT and/or MINIAOD
def switchOnVIDMuonIdProducer(process, dataFormat, task=None):
    process.load('RecoMuon.MuonIdentification.muoMuonIDs_cff')
    if task is not None:
        task.add(process.muoMuonIDTask)
     #*always* reset to an empty configuration                                                                    
    if( len(process.muoMuonIDs.physicsObjectIDs) > 0 ):
        process.muoMuonIDs.physicsObjectIDs = cms.VPSet()
    dataFormatString = "Undefined"
    if dataFormat == DataFormat.AOD:
        # No reconfiguration is required, default settings are for AOD
        dataFormatString = "AOD"
    elif dataFormat == DataFormat.MiniAOD:
        # If we are dealing with MiniAOD, we overwrite the muon collection
        # name appropriately, for the fragment we just loaded above. 
        process.muoMuonIDs.physicsObjectSrc = cms.InputTag('slimmedMuons')
        dataFormatString = "MiniAOD"
    else:
        raise Exception('InvalidVIDDataFormat', 'The requested data format is different from AOD or MiniAOD')
    #
#    sys.stderr.write('Added \'muoMuonIDs\' to process definition (%s format)!\n' % dataFormatString)

def setupVIDMuonSelection(process,cutflow,patProducer=None):
    moduleName = "muoMuonIDs"
    if not hasattr(process, moduleName):
        raise Exception("VIDProducerNotAvailable", "%s producer not available in process!" % moduleName)
    module = getattr(process, moduleName)

    setupVIDSelection(module, cutflow)
    #add to PAT electron producer if available or specified
    #if hasattr(process,'patMuons') or patProducer is not None:
    #    if patProducer is None:
    #        patProducer = process.patMuons
    #    idName = cutflow.idName.value()
    #    addVIDSelectionToPATProducer(patProducer, moduleName, idName)

####
# Photons
####

#turns on the VID photon ID producer, possibly with extra options
# for PAT and/or MINIAOD
def switchOnVIDPhotonIdProducer(process, dataFormat, task=None):
    from RecoEgamma.PhotonIdentification.egmPhotonIDs_cff import  loadEgmIdSequence
    # Set up the ID task and sequence appropriate for this data format
    loadEgmIdSequence(process,dataFormat)
    if task is not None:
        task.add(process.egmPhotonIDTask)
    #*always* reset to an empty configuration
    if( len(process.egmPhotonIDs.physicsObjectIDs) > 0 ):
        process.egmPhotonIDs.physicsObjectIDs = cms.VPSet()
    dataFormatString = "Undefined"
    if dataFormat == DataFormat.AOD:
        # No reconfiguration is required, default settings are for AOD
        dataFormatString = "AOD"
    elif dataFormat == DataFormat.MiniAOD:
        # If we are dealing with MiniAOD, we overwrite the electron collection
        # name appropriately, for the fragment we just loaded above. 
        process.egmPhotonIDs.physicsObjectSrc = cms.InputTag('slimmedPhotons')
        dataFormatString = "MiniAOD"
    else:
        raise Exception('InvalidVIDDataFormat', 'The requested data format is different from AOD or MiniAOD')
    #    
#    sys.stderr.write('Added \'egmPhotonIDs\' to process definition (%s format)!\n' % dataFormatString)

def setupVIDPhotonSelection(process,cutflow,patProducer=None,addUserData=True,task=None):
    if not hasattr(process,'egmPhotonIDs'):
        raise Exception('VIDProducerNotAvailable','egmPhotonIDs producer not available in process!\n')
    setupVIDSelection(process.egmPhotonIDs,cutflow)
    #add to PAT photon producer if available or specified
    if hasattr(process,'patPhotons') or patProducer is not None:
        if patProducer is None:
            patProducer = process.patPhotons
        idName = cutflow.idName.value()
        addVIDSelectionToPATProducer(patProducer,'egmPhotonIDs',idName,addUserData)
        

