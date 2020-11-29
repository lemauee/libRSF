/***************************************************************************
 * libRSF - A Robust Sensor Fusion Library
 *
 * Copyright (C) 2018 Chair of Automation Technology / TU Chemnitz
 * For more information see https://www.tu-chemnitz.de/etit/proaut/self-tuning
 *
 * libRSF is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libRSF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libRSF.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Tim Pfeifer (tim.pfeifer@etit.tu-chemnitz.de)
 ***************************************************************************/

/**
 * @file IV19_GNSS.cpp
 * @author Tim Pfeifer
 * @date 04 Jun 2019
 * @brief File containing an application for 3D pose estimation based on pseudo range measurements. This special version is made for the Intelligent Vehicles Symposium 2019.
 * @copyright GNU Public License.
 *
 */

#include "IV19_GNSS.h"

/** @brief Build the factor Graph with initial values and a first set of measurements
 *
 * @param Graph reference to the factor graph object
 * @param Measurements reference to the dataset that contains the pseudo range measurements
 * @param Config reference to the factor graph config
 * @param Options solver option to estimate initial values
 * @param TimestampFirst first timestamp in the Dataset
 * @return nothing
 *
 */
void InitGraph(libRSF::FactorGraph &Graph,
               libRSF::SensorDataSet &Measurements,
               libRSF::FactorGraphConfig const &Config,
               ceres::Solver::Options Options,
               double TimestampFirst)
{
  /** build simple graph */
  libRSF::FactorGraphConfig SimpleConfig = Config;
  SimpleConfig.GNSS.ErrorModel.Type = libRSF::ErrorModelType::Gaussian;
  libRSF::FactorGraph SimpleGraph;

  SimpleGraph.addState(POSITION_STATE, libRSF::StateType::Point3, TimestampFirst);
  SimpleGraph.addState(CLOCK_ERROR_STATE, libRSF::StateType::ClockError, TimestampFirst);
  AddPseudorangeMeasurements(SimpleGraph, Measurements, SimpleConfig, TimestampFirst);

  /** solve */
  Options.minimizer_progress_to_stdout = false;
  SimpleGraph.solve(Options);

  /**add first state variables */
  Graph.addState(POSITION_STATE, libRSF::StateType::Point3, TimestampFirst);
  Graph.addState(CLOCK_ERROR_STATE, libRSF::StateType::ClockError, TimestampFirst);
  Graph.addState(ORIENTATION_STATE, libRSF::StateType::Angle, TimestampFirst);
  Graph.addState(CLOCK_DRIFT_STATE, libRSF::StateType::ClockDrift, TimestampFirst);

  /** copy values to real graph */
  Graph.getStateData().getElement(POSITION_STATE, TimestampFirst).setMean(SimpleGraph.getStateData().getElement(POSITION_STATE, TimestampFirst).getMean());
  Graph.getStateData().getElement(CLOCK_ERROR_STATE, TimestampFirst).setMean(SimpleGraph.getStateData().getElement(CLOCK_ERROR_STATE, TimestampFirst).getMean());

  /** add first set of measurements */
  AddPseudorangeMeasurements(Graph, Measurements, Config, TimestampFirst);
}

/** @brief Adds a pseudorange measurement to the graph
 *
 * @param Graph reference to the factor graph object
 * @param Measurements reference to the dataset that contains the measurement
 * @param Config reference to the factor graph config object that specifies the motion model
 * @param Timestamp a double timestamp of the current position
 * @return nothing
 *
 */
void AddPseudorangeMeasurements(libRSF::FactorGraph &Graph,
                                libRSF::SensorDataSet &Measurements,
                                libRSF::FactorGraphConfig const &Config,
                                double Timestamp)
{
  libRSF::StateList ListPseudorange;
  libRSF::SensorData Pseudorange;
  libRSF::GaussianDiagonal<1> NoisePseudorange;

  ListPseudorange.add(POSITION_STATE, Timestamp);
  ListPseudorange.add(CLOCK_ERROR_STATE, Timestamp);

  int SatNumber = Measurements.countElement(libRSF::SensorType::Pseudorange3, Timestamp);

  for(int SatCounter = 0; SatCounter < SatNumber; ++SatCounter)
  {
    /** get measurement */
    Pseudorange = Measurements.getElement(libRSF::SensorType::Pseudorange3, Timestamp, SatCounter);

    /** add factor */
    switch(Config.GNSS.ErrorModel.Type)
    {
      case libRSF::ErrorModelType::Gaussian:
        NoisePseudorange.setStdDevDiagonal(Pseudorange.getStdDev());
        Graph.addFactor<libRSF::FactorType::Pseudorange3_ECEF>(ListPseudorange, Pseudorange, NoisePseudorange);
        break;

      case libRSF::ErrorModelType::DCS:
        NoisePseudorange.setStdDevDiagonal(Pseudorange.getStdDev());
        Graph.addFactor<libRSF::FactorType::Pseudorange3_ECEF>(ListPseudorange, Pseudorange, NoisePseudorange, new libRSF::DCSLoss(1.0));
        break;

      case libRSF::ErrorModelType::cDCE:
        NoisePseudorange.setStdDevDiagonal(libRSF::Matrix11::Ones());
        Graph.addFactor<libRSF::FactorType::Pseudorange3_ECEF>(ListPseudorange, Pseudorange, NoisePseudorange, new libRSF::cDCELoss(Pseudorange.getStdDev()[0]));
        break;

      case libRSF::ErrorModelType::GMM:
        static libRSF::GaussianMixture<1> GMM;
        if (GMM.getNumberOfComponents() == 0)
        {
          if (Config.GNSS.ErrorModel.TuningType == libRSF::ErrorModelTuningType::VBI)
          {
            GMM.initSpread(2, 10);/**< number of components is unknown, so choose 2 */
          }
          else
          {
            GMM.initSpread(GMM_N, 10);/**< number of components is known */
          }
        }

        if (Config.GNSS.ErrorModel.MixtureType == libRSF::ErrorModelMixtureType::MaxMix)
        {
          static libRSF::MaxMix1 NoisePseudorangeMaxMix(GMM);
          Graph.addFactor<libRSF::FactorType::Pseudorange3_ECEF>(ListPseudorange, Pseudorange, NoisePseudorangeMaxMix);
        }
        else if (Config.GNSS.ErrorModel.MixtureType == libRSF::ErrorModelMixtureType::SumMix)
        {
          static libRSF::SumMix1 NoisePseudorangeSumMix(GMM);
          Graph.addFactor<libRSF::FactorType::Pseudorange3_ECEF>(ListPseudorange, Pseudorange, NoisePseudorangeSumMix);
        }
        else
        {
          PRINT_ERROR("Wrong error model mixture type!");
        }

        break;

      default:
        PRINT_ERROR("Wrong error model type: ", Config.GNSS.ErrorModel.Type);
        break;
    }
  }
}

/** @brief Use a GMM to estimate the error distribution of a factor
*
* @param Graph reference to the factor graph object
* @param Config reference to the factor graph config object that specifies the motion model
* @return nothing
*
*/
void TuneErrorModel(libRSF::FactorGraph &Graph,
                    libRSF::FactorGraphConfig &Config)
{
  if(Config.GNSS.ErrorModel.TuningType != libRSF::ErrorModelTuningType::None)
  {
    /** compute resudiuals of the factor graph */
    std::vector<double> ErrorData;
    Graph.computeUnweightedError(libRSF::FactorType::Pseudorange3_ECEF, ErrorData);

    if(Config.GNSS.ErrorModel.TuningType == libRSF::ErrorModelTuningType::EM)
    {
      /** fill empty GMM */
      libRSF::GaussianMixture<1> GMM;

      if(GMM.getNumberOfComponents() == 0)
      {
        GMM.initSpread(GMM_N, 10);
      }

      /** call the EM algorithm */
      libRSF::GaussianMixture<1>::EstimationConfig GMMConfig;
      GMMConfig.EstimationAlgorithm = libRSF::ErrorModelTuningType::EM;
      GMMConfig.RemoveSmallComponents = false;
      GMMConfig.MergeSimiliarComponents = false;
      GMM.estimate(ErrorData, GMMConfig);

      /** remove offset of the first "LOS" component */
      GMM.removeOffset();

      /** apply error model */
      if(Config.GNSS.ErrorModel.MixtureType == libRSF::ErrorModelMixtureType::SumMix)
      {
        libRSF::SumMix1 NewSMModel(GMM);
        Graph.setNewErrorModel(libRSF::FactorType::Pseudorange3_ECEF, NewSMModel);
      }
      else if(Config.GNSS.ErrorModel.MixtureType == libRSF::ErrorModelMixtureType::MaxMix)
      {
        libRSF::MaxMix1 NewMMModel(GMM);
        Graph.setNewErrorModel(libRSF::FactorType::Pseudorange3_ECEF, NewMMModel);
      }

    }
    else if(Config.GNSS.ErrorModel.TuningType == libRSF::ErrorModelTuningType::VBI)
    {
      /** initialize GMM */
      static libRSF::GaussianMixture<1> GMMAdaptive;
      libRSF::GaussianComponent<1> Component;

      if(GMMAdaptive.getNumberOfComponents() == 0)
      {
        GMMAdaptive.initSpread(1, 10);
      }

      /** calculate statistics for GMM initialization*/
      std::vector<double> v = ErrorData;
      double sum = std::accumulate(v.begin(), v.end(), 0.0);
      double mean = sum / v.size();

      std::vector<double> diff(v.size());
      std::transform(v.begin(), v.end(), diff.begin(), [mean](double x)
      {
        return x - mean;
      });
      double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
      double stdev = std::sqrt(sq_sum / v.size());

      /** add just one component per timestamp */
      if(GMMAdaptive.getNumberOfComponents() >= VBI_N_MAX)
      {
        GMMAdaptive.sortComponentsByWeight();
        GMMAdaptive.removeLastComponent();
      }

      Component.setParamsStdDev((libRSF::Vector1() << stdev).finished(),
                                (libRSF::Vector1() << 0.0).finished(),
                                (libRSF::Vector1() << 1.0/GMMAdaptive.getNumberOfComponents()).finished());


      GMMAdaptive.addComponent(Component);

      /** estimate GMM with Variational Inference */
      libRSF::GaussianMixture<1>::EstimationConfig GMMConfig;
      GMMConfig.EstimationAlgorithm = libRSF::ErrorModelTuningType::VBI;
      GMMConfig.RemoveSmallComponents = true;
      GMMConfig.MergeSimiliarComponents = false;
      GMMConfig.PriorWishartDOF = VBI_NU;
      GMMAdaptive.estimate(ErrorData, GMMConfig);

      /** remove offset*/
      GMMAdaptive.removeOffset();

      /** apply error model */
      if(Config.GNSS.ErrorModel.MixtureType == libRSF::ErrorModelMixtureType::SumMix)
      {
        libRSF::SumMix1 NewSMModel(GMMAdaptive);
        Graph.setNewErrorModel(libRSF::FactorType::Pseudorange3_ECEF, NewSMModel);
      }
      else if(Config.GNSS.ErrorModel.MixtureType == libRSF::ErrorModelMixtureType::MaxMix)
      {
        libRSF::MaxMix1 NewMMModel(GMMAdaptive);
        Graph.setNewErrorModel(libRSF::FactorType::Pseudorange3_ECEF, NewMMModel);
      }
    }
  }
}

bool ParseErrorModel(const std::string &ErrorModel, libRSF::FactorGraphConfig &Config)
{
  if(ErrorModel.compare("gauss") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::Gaussian;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::None;
  }
  else if(ErrorModel.compare("dcs") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::DCS;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::None;
  }
  else if(ErrorModel.compare("cdce") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::cDCE;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::None;
  }
  else if(ErrorModel.compare("sm") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::GMM;
    Config.GNSS.ErrorModel.MixtureType = libRSF::ErrorModelMixtureType::SumMix;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::None;
  }
  else if(ErrorModel.compare("mm") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::GMM;
    Config.GNSS.ErrorModel.MixtureType = libRSF::ErrorModelMixtureType::MaxMix;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::None;
  }
  else if(ErrorModel.compare("stsm") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::GMM;
    Config.GNSS.ErrorModel.MixtureType = libRSF::ErrorModelMixtureType::SumMix;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::EM;
  }
  else if(ErrorModel.compare("stmm") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::GMM;
    Config.GNSS.ErrorModel.MixtureType = libRSF::ErrorModelMixtureType::MaxMix;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::EM;
  }
  else if(ErrorModel.compare("stsm_vbi") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::GMM;
    Config.GNSS.ErrorModel.MixtureType = libRSF::ErrorModelMixtureType::SumMix;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::VBI;
  }
  else if(ErrorModel.compare("stmm_vbi") == 0)
  {
    Config.GNSS.ErrorModel.Type = libRSF::ErrorModelType::GMM;
    Config.GNSS.ErrorModel.MixtureType = libRSF::ErrorModelMixtureType::MaxMix;
    Config.GNSS.ErrorModel.TuningType = libRSF::ErrorModelTuningType::VBI;
  }
  else
  {
    PRINT_ERROR("Wrong Error Model: ", ErrorModel);
    return false;
  }

  return true;
}

int main(int argc, char** argv)
{
  google::InitGoogleLogging(argv[0]);
  libRSF::FactorGraphConfig Config;

  /** assign all arguments to string vector*/
  std::vector<std::string> Arguments;
  Arguments.assign(argv+1, argv + argc);

  /** read filenames */
  Config.InputFile = Arguments.at(0);
  Config.OutputFile = Arguments.at(1);

  /** parse the error model string */
  if (ParseErrorModel(Arguments.at(3), Config) == false)
  {
    return 1;
  }

  /** configure the solver */
  ceres::Solver::Options SolverOptions;
  SolverOptions.minimizer_progress_to_stdout = false;
  SolverOptions.use_nonmonotonic_steps = true;
  SolverOptions.trust_region_strategy_type = ceres::TrustRegionStrategyType::DOGLEG;
  SolverOptions.dogleg_type = ceres::DoglegType::SUBSPACE_DOGLEG;
  SolverOptions.num_threads = std::thread::hardware_concurrency();
  SolverOptions.max_num_iterations = 100;

  /** read input data */
  libRSF::SensorDataSet InputData;
  libRSF::ReadDataFromFile(Config.InputFile, InputData);

  /** Build optimization problem from sensor data */
  libRSF::FactorGraph Graph;
  libRSF::StateDataSet Result;

  double Timestamp, TimestampFirst = 0.0, TimestampOld, TimestampLast;
  InputData.getTimeFirst(libRSF::SensorType::Pseudorange3, TimestampFirst);
  InputData.getTimeLast(libRSF::SensorType::Pseudorange3, TimestampLast);
  Timestamp = TimestampFirst;
  TimestampOld = TimestampFirst;
  int nTimestamp = 0;

  /** add fist variables and factors */
  InitGraph(Graph, InputData, Config, SolverOptions, TimestampFirst);

  /** solve multiple times with refined model to achieve good initial convergence */
  Graph.solve(SolverOptions);
  TuneErrorModel(Graph, Config);
  Graph.solve(SolverOptions);

  /** safe result at first timestamp */
  Result.addElement(POSITION_STATE, Graph.getStateData().getElement(POSITION_STATE, Timestamp, 0));

  /** get odometry noise from first measurement */
  libRSF::SensorData Odom = InputData.getElement(libRSF::SensorType::Odom3, Timestamp);
  libRSF::Vector4 StdOdom4DOF;
  StdOdom4DOF << Odom.getStdDev().head(3), Odom.getStdDev().tail(1);
  libRSF::GaussianDiagonal<4> NoiseOdom4DOF;
  NoiseOdom4DOF.setStdDevDiagonal(StdOdom4DOF);

  /** hard coded constant clock error drift (CCED) model noise properties */
  libRSF::Vector2 StdCCED;

  if(Config.InputFile.compare("Chemnitz_Input.txt") == 0)
  {
    StdCCED << 0.1, 0.009; /** set CCED standard deviation for Chemnitz City dataset */
  }
  else
  {
    StdCCED << 0.05, 0.01; /** set CCED standard deviation for smartLoc datasets */
  }
  libRSF::GaussianDiagonal<2> NoiseCCED;
  NoiseCCED.setStdDevDiagonal(StdCCED);

  /** iterate over timestamps */
  while(InputData.getTimeNext(libRSF::SensorType::Pseudorange3, Timestamp, Timestamp))
  {
    /** add position, orientation and clock error */
    Graph.addState(POSITION_STATE, libRSF::StateType::Point3, Timestamp);
    Graph.addState(CLOCK_ERROR_STATE, libRSF::StateType::ClockError, Timestamp);
    Graph.addState(ORIENTATION_STATE, libRSF::StateType::Angle, Timestamp);
    Graph.addState(CLOCK_DRIFT_STATE, libRSF::StateType::ClockDrift, Timestamp);

    /** add odometry */
    libRSF::StateList MotionList;
    MotionList.add(POSITION_STATE, TimestampOld);
    MotionList.add(ORIENTATION_STATE, TimestampOld);
    MotionList.add(POSITION_STATE, Timestamp);
    MotionList.add(ORIENTATION_STATE, Timestamp);
    Graph.addFactor<libRSF::FactorType::Odom4_ECEF>(MotionList, InputData.getElement(libRSF::SensorType::Odom3, Timestamp), NoiseOdom4DOF);

    /** add clock drift model */
    libRSF::StateList ClockList;
    ClockList.add(CLOCK_ERROR_STATE, TimestampOld);
    ClockList.add(CLOCK_DRIFT_STATE, TimestampOld);
    ClockList.add(CLOCK_ERROR_STATE, Timestamp);
    ClockList.add(CLOCK_DRIFT_STATE, Timestamp);
    Graph.addFactor<libRSF::FactorType::ConstDrift1>(ClockList, NoiseCCED);

    /** add all pseudo range measurements of with current timestamp */
    AddPseudorangeMeasurements(Graph, InputData, Config, Timestamp);

    /** tune self-tuning error model */
    TuneErrorModel(Graph, Config);

    /** solve the estimation problem */
    Graph.solve(SolverOptions);

    /** save data after optimization */
    Result.addElement(POSITION_STATE, Graph.getStateData().getElement(POSITION_STATE, Timestamp, 0));

    /** apply sliding window */
    Graph.removeAllStatesOutsideWindow(60, Timestamp);

    /** save time stamp */
    TimestampOld = Timestamp;

    nTimestamp++;
  }

  /** print last report */
  Graph.printReport();

  /** write results to disk */
  libRSF::WriteDataToFile(Config.OutputFile, POSITION_STATE, Result);

  return 0;
}