#pragma once 
//umbrella

#include <memory>
#include <atomic>
#include "Candy/Core/CSVWriter.hpp"
#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/Frame/FrameIterator.hpp"
#include "Candy/Core/Frame/FramePacket.hpp"
#include "Candy/Core/CANBundle.hpp"
#include "Candy/Core/CANHelpers.hpp"
#include "Candy/Core/Signal/SignalCodec.hpp"
#include "Candy/Core/Signal/NumericValue.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"

#ifndef CANDY_BUILD_CORE_ONLY

#include "Candy/DBCInterpreters/DBC/DBCInterpreter.hpp"
#include "Candy/DBCInterpreters/DBC/DBCParser.hpp"
#include "Candy/DBCInterpreters/DBC/DBCInterpreterConcepts.hpp"
#include "Candy/DBCInterpreters/MotecGenerator.hpp"
#include "Candy/DBCInterpreters/LoggingTranscoder.hpp"
#include "Candy/DBCInterpreters/V2C/TranslatedMultiplexer.hpp"
#include "Candy/DBCInterpreters/V2C/TranslatedMessage.hpp"
#include "Candy/DBCInterpreters/V2C/TranslatedSignal.hpp"
#include "Candy/DBCInterpreters/V2C/SignalAssembly.hpp"
#include "Candy/DBCInterpreters/V2C/TransmissionGroup.hpp"
#include "Candy/DBCInterpreters/File/FileTranscoder.hpp"
#include "Candy/DBCInterpreters/File/FileTranscoderConcepts.hpp"
#include "Candy/DBCInterpreters/SQLTranscoder.hpp"
#include "Candy/DBCInterpreters/CSVTranscoder.hpp"
#include "Candy/DBCInterpreters/V2CTranscoder.hpp"

#endif // CANDY_BUILD_CORE_ONLY