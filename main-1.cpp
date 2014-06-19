/*******************************************************************************
This file is part of piccante.

piccante is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

piccante is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with piccante.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#define _USE_MATH_DEFINES

#include <mpi.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <malloc.h>
#include <cmath>
#include <iomanip>
#include <cstring>
#include <ctime>       /* time */
#if defined(_MSC_VER)
#include "gsl/gsl_rng.h" // gnu scientific linux per generatore di numeri casuali
#include "gsl/gsl_randist.h"
#else
#include <gsl/gsl_rng.h> // gnu scientific linux per generatore di numeri casuali
#include <gsl/gsl_randist.h>
#endif
#include <cstdarg> //Per chiamare funzioni con numero variabile di argomenti
#include <vector>

using namespace std;

#define DIMENSIONALITY 2

#include "access.h"
#include "commons.h"
#include "grid.h"
#include "structures.h"
#include "current.h"
#include "em_field.h"
#include "particle_species.h"
#include "output_manager.h"

#define NPROC_ALONG_Y 512
#define NPROC_ALONG_Z 1

#define _RESTART_FROM_DUMP 1
#define _DO_RESTART false
#define DO_DUMP false
#define TIME_BTW_DUMP 10

#define DIRECTORY_OUTPUT "TEST"
#define DIRECTORY_DUMP "DUMP"
#define RANDOM_NUMBER_GENERATOR_SEED 5489
#define FREQUENCY_STDOUT_STATUS 5

#define _FACT 0.333333

int main(int narg, char **args)
{
	GRID grid;
	EM_FIELD myfield;
	CURRENT current;
	std::vector<SPECIE*> species;
	vector<SPECIE*>::const_iterator spec_iterator;
    gsl_rng* rng = gsl_rng_alloc(gsl_rng_ranlxd1);

	//*******************************************INIZIO DEFINIZIONE GRIGLIA*******************************************************

    grid.setXrange(-25.0, 20.0);
    grid.setYrange(-7.5, 7.5);
    grid.setZrange(-1, +1);

    grid.setNCells(9180, 3072, 1);
    grid.setNProcsAlongY(NPROC_ALONG_Y);
    grid.setNProcsAlongZ(NPROC_ALONG_Z);

    //grid.enableStretchedGrid();
    //grid.setXandNxLeftStretchedGrid(-20.0,1000);
    grid.setYandNyLeftStretchedGrid(-8.0,21);
    //grid.setXandNxRightStretchedGrid(20.0,1000);
    grid.setYandNyRightStretchedGrid(8.0,21);

    grid.setBoundaries(xPBC | yPBC | zPBC); //LUNGO Z c'è solo PBC al momento !
	grid.mpi_grid_initialize(&narg, args);
	grid.setCourantFactor(0.98);

    grid.setSimulationTime(30.0);

    grid.with_particles = YES;//NO;
    grid.with_current = YES;//YES;

    // grid.setStartMovingWindow(0);
    //grid.setBetaMovingWindow(1.0);
    //grid.setFrequencyMovingWindow(20);

    grid.setMasterProc(0);	
    
    srand(time(NULL));
    grid.initRNG(rng, RANDOM_NUMBER_GENERATOR_SEED);
    
    grid.finalize();
    
    grid.visualDiag();
    
    //********************************************FINE DEFINIZIONE GRIGLIA********************************************************
    
    //*******************************************INIZIO DEFINIZIONE CAMPI*********************************************************
    myfield.allocate(&grid);
    myfield.setAllValuesToZero();
    
    laserPulse pulse1;
    pulse1.type = COS2_PLANE_WAVE;                        //Opzioni : GAUSSIAN, PLANE_WAVE, COS2_PLANE_WAVE
    pulse1.polarization = CIRCULAR_POLARIZATION;
    pulse1.t_FWHM = 12.4;
    pulse1.laser_pulse_initial_position = -12.5;
    pulse1.lambda0 = 1.0;
    pulse1.normalized_amplitude = 198.0*_FACT;
    pulse1.waist = 4.0;
    pulse1.focus_position = 0.0;
    pulse1.rotation = false;
    pulse1.angle = 2.0*M_PI*(-30.0 / 360.0);
    pulse1.rotation_center_along_x = 0.0;

    myfield.addPulse(&pulse1);

    laserPulse pulse2;
    pulse2 = pulse1;
    pulse2.angle = 2.0*M_PI*(30.0 / 360.0);

    //myfield.addPulse(&pulse2);

	myfield.boundary_conditions();

	current.allocate(&grid);
	current.setAllValuesToZero();
	//*******************************************FINE DEFINIZIONE CAMPI***********************************************************

	//*******************************************INIZIO DEFINIZIONE SPECIE*********************************************************
	PLASMA plasma1;
    plasma1.density_function = box;     
    plasma1.setXRangeBox(0.0,1.0*sqrt(_FACT));      
    plasma1.setYRangeBox(grid.rmin[1],grid.rmax[1]);
    plasma1.setZRangeBox(grid.rmin[2],grid.rmax[2]);
    plasma1.setDensityCoefficient(64.0*sqrt(_FACT));
    
    SPECIE  electrons1(&grid);
    electrons1.plasma = plasma1;
    electrons1.setParticlesPerCellXYZ(9, 9, 1);       //Se < 1 il nPPC viene sostituito con 1
    electrons1.setName("ELE1");
    electrons1.type = ELECTRON;
    electrons1.creation();                            //electrons.isTestSpecies=true disabilita deposizione corrente.
    species.push_back(&electrons1);
    
    
    SPECIE ions1(&grid);
    ions1.plasma = plasma1;
    ions1.setParticlesPerCellXYZ(9, 9, 1);
    ions1.setName("ION1");
    ions1.type = ION;
    ions1.Z = 6.0;
    ions1.A = 12.0;
    ions1.creation();
    species.push_back(&ions1);

    
    tempDistrib distribution;
    distribution.setWaterbag(1.0e-8);

    electrons1.add_momenta(rng,0.0,0.0,0.0,distribution);
    ions1.add_momenta(rng,0.0, 0.0, 0.0, distribution);
    

    for (spec_iterator = species.begin(); spec_iterator != species.end(); spec_iterator++){
        (*spec_iterator)->printParticleNumber();
    }

	//    //*******************************************FINE DEFINIZIONE CAMPI***********************************************************

	//*******************************************INIZIO DEFINIZIONE DIAGNOSTICHE**************************************************
    OUTPUT_MANAGER manager(&grid, &myfield, &current, species);

    
    outDomain *plane2= new outDomain;
    plane2->setPointCoordinate(0,0,0);
    plane2->setFreeDimensions(1 ,1,1);
    plane2->setName("SUBD");
    plane2->setXRange(0,6);
    
       
    manager.addSpeciesDensityFrom(plane2,electrons1.name, 0.0, 0.25);
    manager.addSpeciesDensityFrom(plane2,ions1.name, 0.0, 0.25);
    
    manager.addDiagFrom(0.0, 5.0);
    
    manager.initialize(DIRECTORY_OUTPUT);
    //*******************************************FINE DEFINIZIONE DIAGNOSTICHE**************************************************
    grid.setDumpPath(DIRECTORY_DUMP);
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ CICLO PRINCIPALE (NON MODIFICARE) @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	if (grid.myid == grid.master_proc){
		printf("----- START temporal cicle -----\n");
		fflush(stdout);
	}

	int Nstep = grid.getTotalNumberOfTimesteps();
    int dumpID=1, dumpEvery=40;
    grid.istep=0;
    if(DO_DUMP){
        dumpEvery= (int)TIME_BTW_DUMP/grid.dt;
    }
    if (_DO_RESTART){
        dumpID=_RESTART_FROM_DUMP;
        std::ifstream dumpFile;
        dumpFile.open( grid.composeDumpFileName(dumpID).c_str() );
        if( dumpFile.good()){
            grid.reloadDump(dumpFile);
            myfield.reloadDump(dumpFile);
            for (spec_iterator = species.begin(); spec_iterator != species.end(); spec_iterator++){
                (*spec_iterator)->reloadDump(dumpFile);
            }
            dumpFile.close();
            dumpID++;
        }
    }
    while(grid.istep <= Nstep)
    {

        grid.printTStepEvery(FREQUENCY_STDOUT_STATUS);

        manager.callDiags(grid.istep);  /// deve tornare all'inizo del ciclo

        myfield.openBoundariesE_1();
        myfield.new_halfadvance_B();
        myfield.boundary_conditions();

        current.setAllValuesToZero();
        for (spec_iterator = species.begin(); spec_iterator != species.end(); spec_iterator++){
            (*spec_iterator)->current_deposition_standard(&current);
        }
        current.pbc();

        for (spec_iterator = species.begin(); spec_iterator != species.end(); spec_iterator++){
            (*spec_iterator)->position_parallel_pbc();
        }

        myfield.openBoundariesB();
        myfield.new_advance_E(&current);

        myfield.boundary_conditions();
        myfield.openBoundariesE_2();
        myfield.new_halfadvance_B();
        myfield.boundary_conditions();

        for (spec_iterator = species.begin(); spec_iterator != species.end(); spec_iterator++){
            (*spec_iterator)->momenta_advance(&myfield);
        }

        grid.time += grid.dt;

        grid.move_window();
        myfield.move_window();
        for (spec_iterator = species.begin(); spec_iterator != species.end(); spec_iterator++){
            (*spec_iterator)->move_window();
        }
        grid.istep++;
        if(DO_DUMP){
            if (grid.istep!=0 && !(grid.istep % (dumpEvery))) {
                std::ofstream dumpFile;
                dumpFile.open( grid.composeDumpFileName(dumpID).c_str() );

                grid.dump(dumpFile);
                myfield.dump(dumpFile);
                for (spec_iterator = species.begin(); spec_iterator != species.end(); spec_iterator++){
                    (*spec_iterator)->dump(dumpFile);
                }
                dumpFile.close();
                dumpID++;
            }
        }
    }

    manager.close();
    MPI_Finalize();
    exit(1);

}
