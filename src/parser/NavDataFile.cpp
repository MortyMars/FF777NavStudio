// NAVDATAFILE.CPP DÉFINIT LES MÉTHODES DE MODIFICATION
// DES ENREGISTREMENTS AU FORMAT TEXTE

#include "NavDataFile.h"
#include "StreamFile.h"

namespace ndbl {

    using namespace navdata;

    //**************************************************************************
    template<typename Type>
    bool SaveVector(const StreamFilePtr &writer, const std::vector<Type> &src) {
        if (!writer->putValue<uint32>(src.size()))return false;
        return writer->putData(src.data(), src.size() * sizeof(Type));
    }

    //**************************************************************************
    template<typename Type>
    bool LoadVector(const StreamFilePtr &reader, std::vector<Type> &dst) {
        uint32 size;
        if (!reader->getValue<uint32>(size))return false;
        dst.resize(size);

        return reader->getData(dst.data(), dst.size() * sizeof(Type));
    }

    //**************************************************************************
    bool Index::load(const std::string &inPath, Logger*) {
        mSize = 0;
        mHash = 0;
        mConfig.clear();
        mPoints.clear();
        mWaypoints.clear();
        mNavaids.clear();
        mAirports.clear();
        mRunways.clear();
        mLegSequences.clear();
        mLegs.clear();
        mDepartures.clear();
        mArrivals.clear();
        mApproaches.clear();
        mDepartureTransitions.clear();
        mArrivalTransitions.clear();
        mApproachTransitions.clear();
        mRunwayDepartureTransitions.clear();
        mRunwayArrivalTransitions.clear();
        mAirways.clear();
        mAirwaySegments.clear();
        mAirwaySegmentLegs.clear();
        mRoutes.clear();
        mRouteSegments.clear();

        StreamFilePtr stream = StreamFile::initialize(inPath);
        if (!stream) return false;

        mSize = (uint32) (stream->size());
        mHash = (uint32) (stream->hash());
        if (!LoadVector(stream, mConfig))return false;
        if (!LoadVector(stream, mPoints))return false;
        if (!LoadVector(stream, mWaypoints))return false;
        if (!LoadVector(stream, mNavaids))return false;
        if (!LoadVector(stream, mAirports))return false;
        if (!LoadVector(stream, mRunways))return false;
        if (!LoadVector(stream, mLegSequences))return false;
        if (!LoadVector(stream, mLegs))return false;
        if (!LoadVector(stream, mDepartures))return false;
        if (!LoadVector(stream, mArrivals))return false;
        if (!LoadVector(stream, mApproaches))return false;
        if (!LoadVector(stream, mDepartureTransitions))return false;
        if (!LoadVector(stream, mArrivalTransitions))return false;
        if (!LoadVector(stream, mApproachTransitions))return false;
        if (!LoadVector(stream, mRunwayDepartureTransitions))return false;
        if (!LoadVector(stream, mRunwayArrivalTransitions))return false;
        if (!LoadVector(stream, mAirways))return false;
        if (!LoadVector(stream, mAirwaySegments))return false;
        if (!LoadVector(stream, mAirwaySegmentLegs))return false;
        if (!LoadVector(stream, mRoutes))return false;

        return LoadVector(stream, mRouteSegments) != 0;
    }

    //**************************************************************************
    bool Index::save(const std::string &inPath) {
        StreamFilePtr stream = StreamFile::initialize(inPath, true, true);
        if (!stream)return false;

        if (!SaveVector(stream, mConfig))return false;
        if (!SaveVector(stream, mPoints))return false;
        if (!SaveVector(stream, mWaypoints))return false;
        if (!SaveVector(stream, mNavaids))return false;
        if (!SaveVector(stream, mAirports))return false;
        if (!SaveVector(stream, mRunways))return false;
        if (!SaveVector(stream, mLegSequences))return false;
        if (!SaveVector(stream, mLegs))return false;
        if (!SaveVector(stream, mDepartures))return false;
        if (!SaveVector(stream, mArrivals))return false;
        if (!SaveVector(stream, mApproaches))return false;
        if (!SaveVector(stream, mDepartureTransitions))return false;
        if (!SaveVector(stream, mArrivalTransitions))return false;
        if (!SaveVector(stream, mApproachTransitions))return false;
        if (!SaveVector(stream, mRunwayDepartureTransitions))return false;
        if (!SaveVector(stream, mRunwayArrivalTransitions))return false;
        if (!SaveVector(stream, mAirways))return false;
        if (!SaveVector(stream, mAirwaySegments))return false;
        if (!SaveVector(stream, mAirwaySegmentLegs))return false;
        if (!SaveVector(stream, mRoutes))return false;

        return SaveVector(stream, mRouteSegments);
    }

} // Fin de namespace 'ndbl'

