#ifndef HYMOD_H
#define HYMOD_H

#include <cmath>
#include <vector>
#include "LinearReservoir.hpp"
#include "Pdm03.h"

//! Hymod paramaters struct
/*!
    This structure provides storage for the parameters of the hymod hydrological model
*/

struct hymod_params
{
    double max_storage;     //!< maximum amount of water stored
    double a;               //!< coefficent for distributing runoff and slowflow
    double b;               //!< exponent for flux equation
    double Ks;              //!< slow flow coefficent
    double Kq;              //!< quick flow coefficent
    double n;               //!< number of nash cascades
};

//! Hymod state structure
/*!
    This structure provides storage for the state used by hymod hydrological model at a particular time step.
    Warning: to allow bindings to other languages the storage amounts for the reserviors in the nash cascade
    are stored in a pointer which is not allocated or deallocated by this code. Backing storage for these arrays
    must be provide and managed by the creator of these structures.
*/

struct hymod_state
{
    double storage;             //!< the current water storage of the modeled area
    double groundwater_storage; //!< the current water in the ground water linear reservoir
    double* Sr;                 //!< amount of water in each linear reservoir unsafe for binding suport check latter

    //! Constructuor for hymod state
    /*!
        Default constructor for hymod_state objects. This is necessary for the structure to be usable in a map
        Warning: the value of the Sr pointer must be set before this object is used by the hymod_kernel::run() or hymod()
    */
    hymod_state(double inital_storage = 0.0, double gw_storage = 0.0, double* storage_reservoir_ptr = 0x0) :
        storage(inital_storage), groundwater_storage(gw_storage), Sr(storage_reservoir_ptr)
    {

    }
};

//! Hymod flux structure
/*!
    This structure provides storage for the fluxes generated by hymod at any time step
*/

struct hymod_fluxes
{
    double slow_flow;       //!< The flow exiting slow flow at this time step
    double runnoff;         //!< The caluclated runoff amount for this time step
    double et_loss;         //!< The amount of water lost to

    //! Constructor for hymod fluxes
    /*!
        Default constructor for hymod fluxes this is needed for the structs to be storeable in C++ containers.
    */

    hymod_fluxes(double sf = 0.0, double r = 0.0, double et = 0.0) :
        slow_flow(sf), runnoff(r), et_loss(et)
    {}
};

enum HyModErrorCodes
{
    NO_ERROR = 0,
    MASS_BALANCE_ERROR = 100
};

//! Hymod kernel class
/*!
    This class implements the hymod hydrological model


*/

class hymod_kernel
{
    public:

    //! stub function to simulate losses due to evapotransportation
    static double calc_et(double soil_m, void* et_params)
    {
        return 0.0;
    }

    //! run one time step of hymod
    static int run(
        double dt,
        hymod_params params,        //!< static parameters for hymod
        hymod_state state,          //!< model state
        hymod_state& new_state,     //!< model state struct to hold new model state
        hymod_fluxes& fluxes,       //!< model flux object to hold calculated fluxes
        double input_flux,          //!< the amount water entering the system this time step
        void* et_params)            //!< parameters for the et function
    {

        // initalize the nash cascade
        std::vector<LinearReservoir> nash_cascade;

        nash_cascade.resize(params.n);
        for ( unsigned long i = 0; i < nash_cascade.size(); ++i )
        {
            nash_cascade[i] = LinearReservoir(state.Sr[i], params.max_storage, params.Kq, 86400.0);
        }

        // initalize groundwater reservoir
        LinearReservoir groundwater(state.groundwater_storage, params.max_storage, params.Ks, 86400.0);

        // add flux to the current state
        state.storage += input_flux;

        // calculate fs, runoff and slow
        double fs = (1.0 - pow((1.0 - state.storage/params.max_storage),params.b) );
        double runoff = fs * params.a;
        double slow = fs * (1.0 - params.a );
        double soil_m = state.storage - fs;

        // calculate et
        double et = calc_et(soil_m, et_params);

        // get the slow flow output for this time - ks
        double slow_flow = groundwater.response(slow, dt);

        for(unsigned long int i = 0; i < nash_cascade.size(); ++i)
        {
            runoff = nash_cascade[i].response(runoff, dt);
        }

        // record all fluxs
        fluxes.slow_flow = slow_flow;
        fluxes.runnoff = runoff;
        fluxes.et_loss = et;

        // update new state
        new_state.storage = soil_m - et;
        new_state.groundwater_storage = groundwater.get_storage();
        for ( unsigned long i = 0; i < nash_cascade.size(); ++i )
        {
            new_state.Sr[i] = nash_cascade[i].get_storage();
        }

        return mass_check(params, state, input_flux, new_state, fluxes);

    }

    static int mass_check(const hymod_params& params, const hymod_state& current_state, double input_flux, const hymod_state& next_state, const hymod_fluxes& calculated_fluxes)
    {
        // initalize both mass values from current and next states storage
        double inital_mass = current_state.storage + current_state.groundwater_storage;
        double final_mass = next_state.storage + next_state.groundwater_storage;

        // add the masses of the reservoirs before and after the time step
        for ( int i = 0; i < params.n; ++i)
        {
            inital_mass += current_state.Sr[i];
            final_mass += next_state.Sr[i];
        }

        // increase the inital mass by input value
        inital_mass += input_flux;

        // increase final mass by calculated fluxes
        final_mass += (calculated_fluxes.et_loss + calculated_fluxes.runnoff + calculated_fluxes.slow_flow);

        if ( inital_mass - final_mass > 0.000001 )
        {
            return MASS_BALANCE_ERROR;
        }
        else
        {
            return NO_ERROR;
        }
    }
};

extern "C"
{
    /*!
        C entry point for calling hymod_kernel::run
    */

    inline int hymod(
        double dt,                          //!< size of time step
        hymod_params params,                //!< static parameters for hymod
        hymod_state state,                  //!< model state
        hymod_state* new_state,             //!< model state struct to hold new model state
        hymod_fluxes* fluxes,               //!< model flux object to hold calculated fluxes
        double input_flux,                  //!< the amount water entering the system this time step
        void* et_params)                    //!< parameters for the et function
    {
        return hymod_kernel::run(dt, params, state, *new_state, *fluxes, input_flux, et_params);
    }
}

#endif
