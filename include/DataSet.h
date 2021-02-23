/***************************************************************************
 * libRSF - A Robust Sensor Fusion Library
 *
 * Copyright (C) 2018 Chair of Automation Technology / TU Chemnitz
 * For more information see https://www.tu-chemnitz.de/etit/proaut/libRSF
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
 * @file DataSet.h
 * @author Tim Pfeifer
 * @date 18.09.2018
 * @brief Base for Datasets that are able to store time dependent data streams.
 * @copyright GNU Public License.
 *
 */

#ifndef Data_SET_H
#define Data_SET_H

#include "Messages.h"
#include "Constants.h"

#include <cmath>
#include <chrono>

namespace libRSF
{
  /** one class as base for all lists of something in time */
  template<typename KeyType, typename ObjectType>
  class DataSet
  {
    public:
      DataSet() = default;
      virtual ~DataSet() = default;

      /** chronological list of arbitrary objects*/
      typedef std::multimap<double, ObjectType> DataStream;

      /** unique ID that identifies one object */
      struct UniqueID
      {
        UniqueID() = default;

        UniqueID(const KeyType &IDNew, const double TimestampNew, const int NumberNew = 0): ID(IDNew), Timestamp(TimestampNew), Number(NumberNew)
        {}

        /** is required to compare keys */
        bool operator == (const UniqueID &Other) const
        {
          return ID == Other.ID && Timestamp == Other.Timestamp && Number == Other.Number;
        }

        KeyType ID;
        double Timestamp;
        int Number;
      };
      typedef UniqueID ID;

      /** add an element according to its ID and Timestamp*/
      void addElement(const KeyType &ID, const double &Timestamp, const ObjectType &Object)
      {
        if (!this->checkID(ID))
        {
          DataStream TempStream;
          _DataStreams.emplace(ID, TempStream);
        }

        _DataStreams.at(ID).emplace(Timestamp, Object);
      }

      void removeElement(const KeyType &ID, const double Timestamp, const int Number)
      {
        if (this->checkElement(ID, Timestamp, Number))
        {
          auto It = _DataStreams.at(ID).find(Timestamp);
          std::advance(It, Number);
          _DataStreams.at(ID).erase(It);

          /** erase empty IDs */
          if (_DataStreams.at(ID).empty())
          {
            _DataStreams.erase(ID);
          }
        }
        else
        {
          PRINT_ERROR("Element doesn't exist at: ", Timestamp, " Type: ", ID, " Number: ", Number);
        }
      }

      void removeElement(const KeyType &ID, const double Timestamp)
      {
        if (checkElement(ID, Timestamp))
        {
          _DataStreams.at(ID).erase(Timestamp);

          /** erase empty IDs */
          if (_DataStreams.at(ID).empty())
          {
            _DataStreams.erase(ID);
          }
        }
        else
        {
          PRINT_ERROR("Element doesn't exist at: ", Timestamp, " Type: ", ID);
        }
      }

      void clear()
      {
        _DataStreams.clear();
      }

      /** check if an element exists */
      int countElement(const KeyType &ID, const double Timestamp) const
      {
        if (!this->checkID(ID))
        {
          return 0;
        }
        else
        {
          return _DataStreams.at(ID).count(Timestamp);
        }
      }

      int countElements(const KeyType &ID) const
      {
        if (!this->checkID(ID))
        {
          return 0;
        }
        else
        {
          return _DataStreams.at(ID).size();
        }
      }

      bool checkElement(const KeyType &ID, const double Timestamp, const int Number = 0) const
      {
        return (countElement(ID, Timestamp) > Number);
      }

      bool checkID(const KeyType &ID) const
      {
        return (_DataStreams.find(ID) != _DataStreams.end());
      }

      /** check if any element is stored */
      bool empty()
      {
        return _DataStreams.empty();
      }

      /** access elements */
      ObjectType &getElement(const KeyType &ID, const double Timestamp, const int Number = 0)
      {
        if (checkElement(ID, Timestamp, Number))
        {
          auto It = _DataStreams.at(ID).find(Timestamp);
          std::advance(It, Number);
          return It->second;
        }
        else
        {
          PRINT_ERROR("Element doesn't exist at: ", Timestamp, " Type: ", ID, " Number: ", Number);
          return this->NullObject;
        }
      }

      bool getElement(const KeyType &ID, const double Timestamp, const int Number, ObjectType& Element) const
      {
        if (checkElement(ID, Timestamp, Number))
        {
          auto It = _DataStreams.at(ID).find(Timestamp);
          std::advance(It, Number);
          Element = It->second;
          return true;
        }
        else
        {
          return false;
        }
      }

      bool setElement(const KeyType &ID, const double Timestamp, const int Number, ObjectType& Element)
      {
        if (checkElement(ID, Timestamp, Number))
        {
          auto It = _DataStreams.at(ID).find(Timestamp);
          std::advance(It, Number);
          It->second = Element;
          return true;
        }
        else
        {
          PRINT_ERROR("Element doesn't exist at: ", Timestamp, " Type: ", ID, " Number: ", Number);
          return false;
        }
      }

      bool getTimeFirst(const KeyType &ID, double& Timestamp) const
      {
        if(this->checkID(ID))
        {
          Timestamp = _DataStreams.at(ID).begin()->first;
          return true;
        }
        else
        {
          return false;
        }
      }

      bool getTimeFirstOverall(double& Timestamp) const
      {
        if(_DataStreams.empty() == true)
        {
          PRINT_ERROR("Empty list!");
          return false;
        }

        double TimeFirst = Timestamp;
        Timestamp = NAN_DOUBLE;
        std::vector<KeyType> IDs = this->getKeysAll();
        for(const KeyType &ID: IDs)
        {
          this->getTimeFirst(ID, TimeFirst);
          if(std::isnan(Timestamp) == true || TimeFirst < Timestamp)
          {
            Timestamp = TimeFirst;
          }
        }

        return true;
      }

      bool getTimeLast(const KeyType &ID, double& Timestamp) const
      {
        if(this->checkID(ID))
        {
          Timestamp = std::prev(_DataStreams.at(ID).end())->first;
          return true;
        }
        else
        {
          return false;
        }
      }

      bool getTimeNext(const KeyType &ID, const double Timestamp, double& NextTimeStamp) const
      {
        if(this->checkElement(ID, Timestamp))
        {
          auto It = _DataStreams.at(ID).upper_bound(Timestamp);

          if (It != _DataStreams.at(ID).end())
          {
            NextTimeStamp = It->first;
          }
          else
          {
            return false;
          }
          return true;
        }
        else
        {
          PRINT_ERROR("Key does not exist: ", ID);
          return false;
        }
      }

      bool getTimePrev(const KeyType &ID, const double Timestamp, double& PrevTimeStamp) const
      {

        int NextElements = countElement(ID, Timestamp);

        if(NextElements > 0)
        {
          auto It = _DataStreams.at(ID).find(Timestamp);
          std::advance(It, -(NextElements - 1));

          if (It != _DataStreams.at(ID).begin())
          {
            std::advance(It, -1);
            PrevTimeStamp = It->first;
          }
          else
          {
            return false;
          }
          return true;
        }
        else
        {
          PRINT_ERROR("Key does not exist: ", ID);
          return false;
        }
      }

      bool getTimeAboveOrEqual(const KeyType &ID, const double TimeIn, double& TimeOut) const
      {
        if(!this->checkID(ID))
        {
          PRINT_ERROR("Key does not exist: ", ID);
          return false;
        }

        auto It = _DataStreams.at(ID).lower_bound(TimeIn);

        if (It != _DataStreams.at(ID).end())
        {
          TimeOut = It->first;
          return true;
        }
        else
        {
          return false;
        }
      }

      bool getTimeAbove(const KeyType &ID, const double Timestamp, double& NextTimeStamp) const
      {
        if (!this->checkID(ID))
        {
          PRINT_ERROR("Key does not exist: ", ID);
          return false;
        }

        auto It = _DataStreams.at(ID).upper_bound(Timestamp);

        if (It != _DataStreams.at(ID).end())
        {
          NextTimeStamp = It->first;
          return true;
        }
        else
        {
          return false;
        }
      }

      bool getTimeBelow(const KeyType &ID, const double TimeIn, double& TimeOut) const
      {
        if (!this->checkID(ID))
        {
          PRINT_ERROR("Key does not exist: ", ID);
          return false;
        }

        auto It = _DataStreams.at(ID).lower_bound(TimeIn);

        if (It != _DataStreams.at(ID).begin())
        {
          It--;
          /** return element under Timestamp*/
          TimeOut = It->first;
          return true;
        }
        else if (It == _DataStreams.at(ID).end())
        {
          /** container is empty */
          PRINT_ERROR("List is empty!");
        }
        return false;
      }

      bool getTimeBelowOrEqual(const KeyType &ID, const double TimeIn, double& TimeOut) const
      {
        if (!this->checkID(ID))
        {
          PRINT_ERROR("Key does not exist: ", ID);
          return false;
        }

        auto It = _DataStreams.at(ID).lower_bound(TimeIn);


        if (It != _DataStreams.at(ID).end() || It != _DataStreams.at(ID).begin())
        {
          if (It != _DataStreams.at(ID).end() && It->first == TimeIn)
          {
            TimeOut = It->first;
            return true; /**< equal */
          }

          if (It != _DataStreams.at(ID).begin())
          {
            It--;
            TimeOut = It->first;
            return true; /**< below */
          }

          PRINT_WARNING("Key ", ID, " does not have any element below ", TimeIn, "!");
          return false; /**< there is nothing below */
        }
        else
        {
          /** container is empty */
          PRINT_ERROR("Key does not have any element: ", ID);
          return false;
        }
      }

      bool getTimeCloseTo(const KeyType &ID, const double Timestamp, double& TimestampClose) const
      {
        if (!this->checkID(ID))
        {
          PRINT_ERROR("Key does not exist: ", ID);
          return false;
        }

        auto It = _DataStreams.at(ID).lower_bound(Timestamp);

        if (It->first == Timestamp)
        {
          /** Timestamp equal to existing one*/
          TimestampClose = It->first;
          return true;
        }
        else if (It == _DataStreams.at(ID).end() && It != _DataStreams.at(ID).begin())
        {
          /** last timestamp is the closest one */
          It--;
          TimestampClose = It->first;
          return true;
        }
        else if (It != _DataStreams.at(ID).end() && It == _DataStreams.at(ID).begin())
        {
          /** first timestamp is the closest one */
          TimestampClose = It->first;
          return true;
        }
        else if (It != _DataStreams.at(ID).end() && It != _DataStreams.at(ID).begin())
        {
          /** we have to check both sides */
          double TimeBelow;

          if(this->getTimePrev(ID, It->first, TimeBelow)) /**< this should always be true, since we are somewhere in the middle */
          {
            if((It->first - Timestamp) <= (Timestamp - TimeBelow))
            {
              TimestampClose = It->first;
            }
            else
            {
              TimestampClose = TimeBelow;
            }
            return true;
          }
          else
          {
            PRINT_ERROR("Something gone wrong badly!");
          }
        }
        else /**< this means It == begin == end, so the map is empty */
        {
          PRINT_ERROR("List is empty!");
        }

        return false;
      }

      int countTimes(const KeyType &ID) const
      {
        if(!this->checkID(ID))
        {
          return 0;
        }
        else
        {
          return _DataStreams.at(ID).size();
        }
      }

      std::vector<KeyType> getKeysAll() const
      {
        std::vector<KeyType> Keys;
        for(auto it = _DataStreams.begin(); it != _DataStreams.end(); ++it)
        {
          Keys.push_back(it->first);
        }
        if(Keys.empty())
        {
          PRINT_WARNING("Returned empty vector!");
        }
        return Keys;
      }

      std::vector<KeyType> getKeysAtTime(const double Timestamp) const
      {
        std::vector<KeyType> KeysAll, KeysAtT;
        KeysAll = this->getKeysAll();
        for(const KeyType& Key : KeysAll)
        {
          if (this->checkElement(Key, Timestamp) == true)
          {
            KeysAtT.push_back(Key);
          }
        }
        if(KeysAtT.empty())
        {
          PRINT_WARNING("Returned empty vector!");
        }
        return KeysAtT;
      }

      std::vector<ObjectType> getElementsOfID(const KeyType &ID) const
      {
        std::vector<ObjectType> Objects;

        if (_DataStreams.count(ID) > 0)
        {
          for(auto it = _DataStreams.at(ID).begin(); it != _DataStreams.at(ID).end(); ++it)
          {
            Objects.push_back(it->second);
          }
        }

        if(Objects.empty())
        {
          PRINT_WARNING("Returned empty vector!");
        }
        return Objects;
      }

      std::vector<ObjectType> getElements(const KeyType &ID, const double Timestamp) const
      {
        std::vector<ObjectType> Objects;

        if(this->checkID(ID))
        {
          auto Range = _DataStreams.at(ID).equal_range(Timestamp);
          for (auto it = Range.first; it != Range.second; ++it)
          {
            Objects.push_back(it->second);
          }
        }

        if(Objects.empty())
        {
          PRINT_WARNING("Returned empty vector!");
        }
        return Objects;
      }

      std::vector<ObjectType> getElementsBetween(const KeyType &ID, const double TimeBegin, const double TimeEnd) const
      {
        std::vector<ObjectType> Objects;

        /** catch special case, where timestamps are identical */
        if (TimeBegin == TimeEnd)
        {
          return this->getElements(ID, TimeBegin);
        }

        if(this->checkID(ID))
        {
          /** identify true boarders */
          double TimeFirst, TimeLast;
          if (this->getTimeBelowOrEqual(ID, TimeEnd, TimeLast) == false)
          {
            PRINT_WARNING("Did not find upper bound of ", ID, " at ", TimeBegin);
            return Objects;
          }
          if (this->getTimeAboveOrEqual(ID, TimeBegin, TimeFirst) == false)
          {
            PRINT_WARNING("Did not find lower bound of ", ID, " at ", TimeBegin);
            return Objects;
          }

          if (TimeFirst > TimeLast)
          {
            PRINT_WARNING("There is no object between ", TimeBegin, "s and ", TimeEnd, "s of type ", ID);
            return Objects;
          }

          /** iterate over timestamps */
          double Time = TimeFirst;
          do
          {
            std::vector<ObjectType> CurrentObjects = this->getElements(ID, Time);
            Objects.insert(Objects.end(), CurrentObjects.begin(), CurrentObjects.end());

            /** exit loop if last timestamp is reached */
            if(Time == TimeLast)
              break;
          }
          while(this->getTimeNext(ID, Time, Time));
        }
        else
        {
          PRINT_ERROR("There is no element with type: ", ID);
        }

        if(Objects.empty())
        {
          PRINT_WARNING("Returned empty vector!");
        }

        return Objects;
      }

      bool getUniqueIDs(const KeyType &ID, std::vector<UniqueID> &IDs) const
      {
        if(this->checkID(ID))
        {
          /** loop over timestamps */
          const DataStream &StreamRef = _DataStreams.at(ID);
          for(auto it = StreamRef.begin(); it != StreamRef.end(); it = StreamRef.upper_bound(it->first))
          {
            /** loop over elements at one timestamp */
            for(int n = 0; n < static_cast<int>(this->countElement(ID, it->first)); ++n)
            {
              IDs.push_back(UniqueID(ID, it->first, n));
            }
          }
          return true;
        }
        else
        {
          PRINT_ERROR("There is no ID: ", ID);
          return false;
        }
      }

      bool getTimesOfID(const KeyType &ID, std::vector<double> &Times) const
      {
        if(this->checkID(ID))
        {
          const DataStream &StreamRef = _DataStreams.at(ID);
          for(auto it = StreamRef.begin(); it != StreamRef.end(); it = StreamRef.upper_bound(it->first))
          {
            Times.push_back(it->first);
          }
          return true;
        }
        else
        {
          PRINT_ERROR("There is no ID: ", ID);
          return false;
        }
      }

      bool getTimesBetween(const KeyType &ID, const double StartTime, const double EndTime, std::vector<double> &Times) const
      {
        if(this->checkID(ID))
        {
          double Start, End;
          if(this->_FindBordersEqual(ID, StartTime, EndTime, Start, End))
          {
            double Current = Start;
            while (Current <= End)
            {
              Times.push_back(Current);

              if(this->getTimeNext(ID, Current, Current) == false)
              {
                break; /**< exit if no further timestamp is available */
              }
            }
          }
          else
          {
            PRINT_WARNING("Could not find timestamps between", StartTime, " and ", EndTime, " for ", ID);
            return false;
          }
        }
        else
        {
          PRINT_ERROR("There is no ID: ", ID);
          return false;
        }
        return true;
      }

      bool getTimesBelowOrEqual(const KeyType &ID, const double EndTime, std::vector<double> &Times) const
      {
        double Start;
        if(this->getTimeFirst(ID, Start))
        {
          double End;
          if(this->getTimeBelowOrEqual(ID, EndTime, End))
          {
            double Current = Start;
            bool HasNext = true;
            while (Current <= End && HasNext)
            {
              Times.push_back(Current);
              HasNext = this->getTimeNext(ID, Current, Current);
            }
          }
          else
          {
            PRINT_WARNING("Could not find timestamps before ", EndTime, " for ", ID);
            return false;
          }
        }
        else
        {
          PRINT_ERROR("There is no ID: ", ID);
          return false;
        }
        return true;
      }

      /** functions for range based for-loops */
      auto begin()
      {
        return _DataStreams.begin();
      }

      const auto begin() const
      {
        return _DataStreams.begin();
      }

      auto end()
      {
        return _DataStreams.end();
      }

      const auto end() const
      {
        return _DataStreams.end();
      }

      /** combine two lists*/
      void merge(DataSet<KeyType, ObjectType> &List)
      {
        for (auto& Map: List)
        {
          for (auto& Element: Map.second)
          {
            this->addElement(Map.first, Element.first, Element.second);
          }
        }
      }

    protected:
      bool _FindBordersEqual(const KeyType &ID, const double Start, const double End, double &StartTrue, double &EndTrue) const
      {
        if(Start > End)
        {
          PRINT_ERROR("Start: ", Start, " is grater than End: ", End);
          return false;
        }

        if(!this->getTimeAboveOrEqual(ID, Start, StartTrue))
        {
          PRINT_ERROR("There is no object above: ", Start);
          return false;
        }

        if(!this->getTimeBelowOrEqual(ID, End, EndTrue))
        {
          PRINT_ERROR("There is no object above: ", End);
          return false;
        }
        return true;
      }

      bool _FindBorders(const KeyType &ID, const double Start, const double End, double &StartTrue, double &EndTrue) const
      {
        if(Start > End)
        {
          PRINT_ERROR("Start: ", Start, " is grater than End: ", End);
          return false;
        }

        if(!this->getTimeAbove(ID, Start, StartTrue))
        {
          PRINT_ERROR("There is no object above: ", Start);
          return false;
        }

        if(!this->getTimeBelow(ID, End, EndTrue))
        {
          PRINT_ERROR("There is no object above: ", End);
          return false;
        }
        return true;
      }

      std::map<KeyType, DataStream> _DataStreams;

      /** for empty references */
      ObjectType NullObject;
  };

}

#endif // Data_SET_H
