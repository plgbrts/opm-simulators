/*
  Copyright 2021 SINTEF Digital, Mathematics and Cybernetics.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPM_PARALLELCOMMUNICATION_HEADER_INCLUDED
#define OPM_PARALLELCOMMUNICATION_HEADER_INCLUDED

#include <dune/common/version.hh>
#include <dune/common/parallel/mpihelper.hh>

namespace Opm
{
namespace Parallel
{
    using MPIComm = typename Dune::MPIHelper::MPICommunicator;
#if DUNE_VERSION_NEWER(DUNE_COMMON, 2, 7)
    using Communication = Dune::Communication<MPIComm>;
#else
    using Communication = Dune::CollectiveCommunication<MPIComm>;
#endif
} // namespace Parallel
} // end namespace Opm
#endif // OPM_PARALLELCOMMUNICATION_HEADER_INCLUDED
