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

#include "StateDataSet.h"

namespace libRSF
{
  void StateDataSet::addElement(StateData &Element)
  {
    addElement(Element.getName(),Element);
  }

  void StateDataSet::addElement(std::string Name, StateData &Element)
  {
    if (!this->checkID(Name))
    {
      DataStream TempStream;
      _DataStreams.emplace(Name, TempStream);
    }

    _DataStreams[Name].emplace(Element.getTimestamp(), Element);
  }

  void StateDataSet::addElement(std::string Name, StateType Type, double Timestamp)
  {
    StateData Element(Type, Timestamp);
    addElement(Name, Element);
  }

  std::ostream& operator << (std::ostream& Os, const StateID& ID)
  {
    Os << ID.ID << " " << ID.Timestamp << " " << ID.Number << " ";
    return Os;
  }
}
