/*
  Copyright 2016 IRIS

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

#ifndef OPM_WELLSTATEFULLYIMPLICITBLACKOILDENSE_HEADER_INCLUDED
#define OPM_WELLSTATEFULLYIMPLICITBLACKOILDENSE_HEADER_INCLUDED


#include <opm/core/wells.h>
#include <opm/core/well_controls.h>
#include <opm/core/simulator/WellState.hpp>
#include <opm/autodiff/BlackoilModelEnums.hpp>
#include <opm/autodiff/WellStateFullyImplicitBlackoil.hpp>
#include <opm/core/props/BlackoilPhases.hpp>
#include <opm/common/ErrorMacros.hpp>
#include <vector>
#include <cassert>
#include <string>
#include <utility>
#include <map>
#include <algorithm>
#include <array>
#include <cmath>

namespace Opm
{

    /// The state of a set of wells, tailored for use by the fully
    /// implicit blackoil simulator.
    class WellStateFullyImplicitBlackoilDense
        : public WellStateFullyImplicitBlackoil
    {
        typedef WellStateFullyImplicitBlackoil  BaseType;
    public:
        typedef BaseType :: WellMapType WellMapType;

        using BaseType :: wellRates;
        using BaseType :: bhp;
        using BaseType :: perfPress;
        using BaseType :: wellMap;
        using BaseType :: numWells;
        using BaseType :: numPhases;
        using BaseType :: perfPhaseRates;
        using BaseType :: currentControls;

        /// Allocate and initialize if wells is non-null.  Also tries
        /// to give useful initial values to the bhp(), wellRates()
        /// and perfPhaseRates() fields, depending on controls
        template <class State, class PrevState>
        void init(const Wells* wells, const State& state, const PrevState& prevState, const PhaseUsage& pu)
        {
            // call init on base class
            BaseType :: init(wells, state, prevState);

            // TODO: the reason to keep this is to avoid getting defaulted value BHP
            // some facilities needed from opm-parser or opm-core
            // It is a little tricky, since sometimes before applying group control, the only
            // available constraints in the well_controls is the defaulted BHP value, and it
            // is really not desirable to use this value to enter the Newton iterations.
            setWellSolutions(pu);
        }



        /// Set wellSolutions() based on the base class members.
        void setWellSolutions(const PhaseUsage& pu)
        {
            // Set nw and np, or return if no wells.
            if (wells_.get() == nullptr) {
                return;
            }
            const int nw = wells_->number_of_wells;
            if (nw == 0) {
                return;
            }
            const int np = wells_->number_of_phases;
            well_solutions_.clear();
            well_solutions_.resize(nw * np, 0.0);
            std::vector<double> g = {1.0,1.0,0.01};
            for (int w = 0; w < nw; ++w) {
                WellControls* wc = wells_->ctrls[w];

                // The current control in the well state overrides
                // the current control set in the Wells struct, which
                // is instead treated as a default.
                const int current = currentControls()[w];
                well_controls_set_current( wc, current);
                const WellType& well_type = wells_->type[w];

                switch (well_controls_iget_type(wc, current)) {
                case THP: // Intentional fall-through
                case BHP:
                    if (well_type == INJECTOR) {
                        for (int p = 0; p < np; ++p)  {
                            well_solutions_[w] += wellRates()[np*w + p] * wells_->comp_frac[np*w + p];
                        }
                    } else {
                        for (int p = 0; p < np; ++p) {
                            well_solutions_[w] += g[p] * wellRates()[np*w + p];
                        }
                    }
                    break;
                case RESERVOIR_RATE: // Intentional fall-through
                case SURFACE_RATE:
                    wellSolutions()[w] = bhp()[w];
                    break;
                }

                double total_rates = 0.0;
                for (int p = 0; p < np; ++p)  {
                    total_rates += g[p] * wellRates()[np*w + p];
                }

                const int waterpos = pu.phase_pos[Water];
                const int gaspos = pu.phase_pos[Gas];

                assert(np > 2 || (np == 2 && !pu.phase_used[Gas]));
                // assumes the gas fractions are stored after water fractions
                if(std::abs(total_rates) > 0) {
                    if( pu.phase_used[Water] ) {
                        wellSolutions()[nw + w] = g[Water] * wellRates()[np*w + waterpos] / total_rates;
                    }
                    if( pu.phase_used[Gas] ) {
                        wellSolutions()[2*nw + w] = g[Gas] * wellRates()[np*w + gaspos] / total_rates ;
                    }


                } else {
                    if( pu.phase_used[Water] ) {
                        wellSolutions()[nw + w] = wells_->comp_frac[np*w + waterpos];
                    }
                    if( pu.phase_used[Gas] ) {
                        wellSolutions()[2*nw + w] = wells_->comp_frac[np*w + gaspos];
                    }
                }
            }
        }


        template <class State>
        void resize(const Wells* wells, const State& state, const PhaseUsage& pu ) {
            const WellStateFullyImplicitBlackoilDense dummy_state{}; // Init with an empty previous state only resizes
            init(wells, state, dummy_state, pu) ;
        }


        /// One rate per phase and well connection.
        std::vector<double>& wellSolutions() { return well_solutions_; }
        const std::vector<double>& wellSolutions() const { return well_solutions_; }

        data::Wells report(const PhaseUsage& pu) const override {
            data::Wells res = BaseType::report(pu);
            return res;
        }


    private:
        std::vector<double> well_solutions_;
    };

} // namespace Opm


#endif // OPM_WELLSTATEFULLYIMPLICITBLACKOILDENSE_HEADER_INCLUDED
