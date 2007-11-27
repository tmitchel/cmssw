#include "RecoVertex/AdaptiveVertexFinder/interface/AdaptiveVertexReconstructor.h"
#include "RecoVertex/VertexTools/interface/GeometricAnnealing.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "RecoVertex/VertexTools/interface/DummyVertexSmoother.h"
#include "RecoVertex/AdaptiveVertexFit/interface/KalmanVertexSmoother.h"
#include <algorithm>

using namespace std;

TransientVertex AdaptiveVertexReconstructor::cleanUp ( const TransientVertex & old ) const
{
  vector < reco::TransientTrack > trks = old.originalTracks();
  vector < reco::TransientTrack > newtrks;
  TransientVertex::TransientTrackToFloatMap mp;
  static const float minweight = 1.e-8; // discard all tracks with lower weight
  for ( vector< reco::TransientTrack >::const_iterator i=trks.begin();
        i!=trks.end() ; ++i )
  {
    if ( old.trackWeight ( *i ) > minweight )
    {
      newtrks.push_back ( *i );
      mp[*i]=old.trackWeight ( *i );
    }
  }

  TransientVertex ret;

  if ( old.hasPrior() )
  {
    VertexState priorstate ( old.priorPosition(), old.priorError() );
    ret=TransientVertex ( priorstate, old.vertexState(), newtrks,
        old.totalChiSquared(), old.degreesOfFreedom() );
  } else {
    ret=TransientVertex ( old.vertexState(), newtrks,
                          old.totalChiSquared(), old.degreesOfFreedom() );
  }
  ret.weightMap ( mp ); // set weight map

  if ( old.hasRefittedTracks() )
  {
    // we have refitted tracks -- copy them!
    vector < reco::TransientTrack > newrfs;
    vector < reco::TransientTrack > oldrfs=old.refittedTracks();
    vector < reco::TransientTrack >::const_iterator origtrkiter=trks.begin();
    for ( vector< reco::TransientTrack >::const_iterator i=oldrfs.begin(); i!=oldrfs.end() ; ++i )
    {
      if ( old.trackWeight ( *origtrkiter ) > minweight )
      {
        newrfs.push_back ( *i );
      }
      origtrkiter++;
    }
    if ( newrfs.size() ) ret.refittedTracks ( newrfs ); // copy refitted tracks
  }

  if ( ret.refittedTracks().size() > ret.originalTracks().size() )
  {
    edm::LogError("AdaptiveVertexReconstructor" )
      << "More refitted tracks than original tracks!";
  }

  return ret;
}

void AdaptiveVertexReconstructor::erase (
    const TransientVertex & newvtx,
    set < reco::TransientTrack > & remainingtrks,
    float w ) const
{
  /*
   * Erase tracks that are in newvtx from remainingtrks */
  const vector < reco::TransientTrack > & origtrks = newvtx.originalTracks();
  bool erased=false;
  for ( vector< reco::TransientTrack >::const_iterator i=origtrks.begin();
        i!=origtrks.end(); ++i )
  {
    double weight = newvtx.trackWeight ( *i );
    if ( weight > w )
    {
      remainingtrks.erase ( *i );
      erased=true;
    };
  };
}

AdaptiveVertexReconstructor::AdaptiveVertexReconstructor(
    float primcut, float seccut, float min_weight, bool smoothing ) :
  thePrimaryFitter ( 0 ), theSecondaryFitter ( 0 ),
       theMinWeight( min_weight ), theWeightThreshold ( 0.001 )
{
  setupFitters ( primcut, seccut, smoothing );
}

AdaptiveVertexReconstructor::~AdaptiveVertexReconstructor()
{
  if ( thePrimaryFitter ) delete thePrimaryFitter;
  if ( theSecondaryFitter ) delete theSecondaryFitter;
}

void AdaptiveVertexReconstructor::setupFitters ( float primcut, float seccut, bool smoothing )
{
  VertexSmoother * smoother ;
  if ( smoothing )
  {
    smoother = new KalmanVertexSmoother();
  } else {
    smoother = new DummyVertexSmoother();
  }

  if ( thePrimaryFitter ) delete thePrimaryFitter;
  if ( theSecondaryFitter ) delete theSecondaryFitter;

  thePrimaryFitter = new AdaptiveVertexFitter ( GeometricAnnealing ( primcut ), DefaultLinearizationPointFinder(),
      KalmanVertexUpdator(), KalmanVertexTrackCompatibilityEstimator(), *smoother );
  thePrimaryFitter->setWeightThreshold ( theWeightThreshold );
  // if the primary fails, sth is wrong, so here we set a threshold on the weight.
  theSecondaryFitter = new AdaptiveVertexFitter ( GeometricAnnealing ( seccut ), DefaultLinearizationPointFinder(),
      KalmanVertexUpdator(), KalmanVertexTrackCompatibilityEstimator(), *smoother );
  theSecondaryFitter->setWeightThreshold ( 0. );
  // need to set it or else we have 
  // unwanted exceptions to deal with.
  // cleanup can come later!
  delete smoother;
}

AdaptiveVertexReconstructor::AdaptiveVertexReconstructor( const edm::ParameterSet & m )
  : thePrimaryFitter(0), theSecondaryFitter(0), theMinWeight(0.5), theWeightThreshold ( 0.001 )
{
  float primcut = 2.0;
  float seccut = 6.0;
  bool smoothing=false;
  
  try {
    primcut =  m.getParameter<double>("primcut");
    seccut =  m.getParameter<double>("seccut");
    theMinWeight = m.getParameter<double>("minweight");
    theWeightThreshold = m.getParameter<double>("weightthreshold");
    smoothing =  m.getParameter<bool>("smoothing");
  } catch ( edm::Exception & e ) {
    edm::LogError ("AdaptiveVertexReconstructor") << e.what();
  }

  setupFitters ( primcut, seccut, smoothing );
}

vector<TransientVertex> 
    AdaptiveVertexReconstructor::vertices(const vector<reco::TransientTrack> & t, 
        const reco::BeamSpot & s ) const
{
  return vertices ( t,s, true );
}

vector<TransientVertex> AdaptiveVertexReconstructor::vertices (
    const vector<reco::TransientTrack> & tracks ) const
{
  return vertices ( tracks, reco::BeamSpot() , false );
}

vector<TransientVertex> AdaptiveVertexReconstructor::vertices (
    const vector<reco::TransientTrack> & tracks,
    const reco::BeamSpot & s, bool usespot ) const
{
  vector < TransientVertex > ret;
  set < reco::TransientTrack > remainingtrks;

  copy(tracks.begin(), tracks.end(), 
	    inserter(remainingtrks, remainingtrks.begin()));

  int ctr=0;
  unsigned int n_tracks = remainingtrks.size();

  // cout << "[AdaptiveVertexReconstructor] DEBUG ::vertices!!" << endl;
  try {
    while ( remainingtrks.size() > 1 )
    {
      /*
      cout << "[AdaptiveVertexReconstructor] next round: "
           << remainingtrks.size() << endl;
           */
      ctr++;
      const AdaptiveVertexFitter * fitter = theSecondaryFitter;
      if ( ret.size() == 0 )
      {
        fitter = thePrimaryFitter;
      };
      vector < reco::TransientTrack > fittrks;
      fittrks.reserve ( remainingtrks.size() );

      copy(remainingtrks.begin(), remainingtrks.end(), back_inserter(fittrks));

      TransientVertex tmpvtx;
      if ( (ret.size() == 0) && usespot )
      {
        tmpvtx=fitter->vertex ( fittrks, s );
      } else {
        tmpvtx=fitter->vertex ( fittrks );
      }
      TransientVertex newvtx = cleanUp ( tmpvtx );
      ret.push_back ( newvtx );
      erase ( newvtx, remainingtrks, theWeightThreshold );
      if ( n_tracks == remainingtrks.size() )
      {
        if ( usespot )
        {
          // try once more without beamspot constraint!
          usespot=false;
          LogDebug("AdaptiveVertexReconstructor") 
            << "no tracks in vertex. trying again without beamspot constraint!";
          continue;
        }
        LogDebug("AdaptiveVertexReconstructor") << "all tracks (" << n_tracks
             << ") would be recycled for next fit. Trying with low threshold!";
        erase ( newvtx, remainingtrks, 1.e-5 );
        if ( n_tracks == remainingtrks.size() )
        {
          LogDebug("AdaptiveVertexReconstructor") << "low threshold didnt help! "
                                                  << "Discontinue procedure!";
          break;
        }
      };

      // cout << "[AdaptiveVertexReconstructor] erased" << endl;
      n_tracks = remainingtrks.size();
    };
  } catch ( VertexException & e ) {
    // Will catch all (not enough significant tracks exceptions.
    // in this case, the iteration can safely terminate.
  };

  return cleanUpVertices ( ret );
}

vector<TransientVertex> AdaptiveVertexReconstructor::cleanUpVertices (
    const vector < TransientVertex > & old ) const
{
  vector < TransientVertex > ret;
  for ( vector< TransientVertex >::const_iterator i=old.begin(); i!=old.end() ; ++i )
  {
    if (!(i->hasTrackWeight()))
    { // if we dont have track weights, we take the vtx
      ret.push_back ( *i );
      continue;
    }

    // maybe this should be replaced with asking for the ndf ...
    // e.g. if ( ndf > - 1. )
    int nsig=0; // number of significant tracks. 
    TransientVertex::TransientTrackToFloatMap wm = i->weightMap();
    for ( TransientVertex::TransientTrackToFloatMap::const_iterator w=wm.begin(); w!=wm.end() ; ++w )
    {
      if (w->second > theWeightThreshold) nsig++;
    }
    if ( nsig > 1 ) ret.push_back ( *i );
  }

  return ret;
}
