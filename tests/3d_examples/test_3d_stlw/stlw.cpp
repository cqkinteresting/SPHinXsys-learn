/**
 * @file 	 stlw.h
 * @brief 	 This is the case file for still water.
 * @author   Nicolò Salis
 */
#include "sphinxsys.h" //SPHinXsys Library.
using namespace SPH;
#include "stlw.h" //header for this case

int main(int ac, char *av[])
{
    //----------------------------------------------------------------------
    //	Build up the environment of a SPHSystem with global controls.
    //----------------------------------------------------------------------
    SPHSystem sph_system(system_domain_bounds, particle_spacing_ref);
    sph_system.handleCommandlineOptions(ac, av)->setIOEnvironment();
    //----------------------------------------------------------------------
    //	Creating body, materials and particles.
    //----------------------------------------------------------------------
    FluidBody water_block(sph_system, makeShared<WaterBlock>("WaterBody"));
    water_block.defineParticlesAndMaterial<BaseParticles, WeaklyCompressibleFluid>(rho0_f, c_f, mu_f);
    water_block.generateParticles<ParticleGeneratorLattice>();
    water_block.addBodyStateForRecording<Real>("VolumetricMeasure");

    SolidBody wall_boundary(sph_system, makeShared<WallBoundary>("Wall"));
    wall_boundary.defineParticlesAndMaterial<SolidParticles, Solid>();
    wall_boundary.generateParticles<ParticleGeneratorLattice>();
    //----------------------------------------------------------------------
    //	Define body relation map.
    //	The contact map gives the topological connections between the bodies.
    //	Basically the the range of bodies to build neighbor particle lists.
    //  Generally, we first define all the inner relations, then the contact relations.
    //  At last, we define the complex relaxations by combining previous defined
    //  inner and contact relations.
    //----------------------------------------------------------------------
    InnerRelation water_block_inner(water_block);
    ContactRelation water_wall_contact(water_block, {&wall_boundary});
    //----------------------------------------------------------------------
    // Combined relations built from basic relations
    // which is only used for update configuration.
    //----------------------------------------------------------------------
    ComplexRelation water_block_complex(water_block_inner, water_wall_contact);
    //----------------------------------------------------------------------
    //	Define all numerical methods which are used in this case.
    //----------------------------------------------------------------------
    SimpleDynamics<NormalDirectionFromBodyShape> wall_boundary_normal_direction(wall_boundary);
    /** Time step initialization, add gravity. */
    SimpleDynamics<TimeStepInitialization> initialize_time_step_to_fluid(water_block, makeShared<Gravity>(Vecd(0.0, 0.0, -gravity_g)));
    /** Evaluation of density by summation approach. */
    InteractionWithUpdate<fluid_dynamics::DensitySummationComplexFreeSurface> update_density_by_summation(water_block_inner, water_wall_contact);
    /** time step size without considering sound wave speed. */
    ReduceDynamics<fluid_dynamics::AdvectionTimeStepSize> get_fluid_advection_time_step_size(water_block, U_f);
    /** time step size with considering sound wave speed. */
    ReduceDynamics<fluid_dynamics::AcousticTimeStepSize> get_fluid_time_step_size(water_block);
    /** pressure relaxation using Verlet time stepping. */
    Dynamics1Level<fluid_dynamics::Integration1stHalfWithWallRiemann> pressure_relaxation(water_block_inner, water_wall_contact);
    Dynamics1Level<fluid_dynamics::Integration2ndHalfWithWallRiemann> density_relaxation(water_block_inner, water_wall_contact);
    /** Computing viscous acceleration. */
    InteractionDynamics<fluid_dynamics::ViscousAccelerationWithWall> viscous_acceleration(water_block_inner, water_wall_contact);
    /*-------------------------------------------------------------------------------*/
    /*--------------------------FREE SURFACE IDENTIFICATION--------------------------*/
    /*-------------------------------------------------------------------------------*/
    InteractionWithUpdate<SpatialTemporalFreeSurfaceIndicationComplex>
        free_stream_surface_indicator(water_block_inner, water_wall_contact);
    /** Impose transport velocity formulation. */
    InteractionWithUpdate<fluid_dynamics::TransportVelocityCorrectionComplex<BulkParticles>>
        transport_velocity_correction(water_block_inner, water_wall_contact);
    /*-------------------------------------------------------------------------------*/
    //----------------------------------------------------------------------
    //	Define the methods for I/O operations and observations of the simulation.
    //----------------------------------------------------------------------
    BodyStatesRecordingToVtp write_real_body_states(sph_system.real_bodies_);
    /** WaveProbes. */
    BodyRegionByCell wave_probe_buffer(water_block, makeShared<TransformShape<GeometricShapeBox>>(Transform(translation_FS_gauge), FS_gauge));
    RegressionTestDynamicTimeWarping<ReducedQuantityRecording<UpperFrontInAxisDirection<BodyPartByCell>>>
        wave_gauge(wave_probe_buffer, "FreeSurfaceHeight");
    //----------------------------------------------------------------------
    //	Basic control parameters for time stepping.
    //----------------------------------------------------------------------
    GlobalStaticVariables::physical_time_ = 0.0;
    int number_of_iterations = 0;
    int screen_output_interval = 1000;
    Real end_time = total_physical_time;
    Real output_interval = end_time / 200;
    Real dt = 0.0;
    Real total_time = 0.0;
    Real relax_time = 1.0;
    /** statistics for computing time. */
    TickCount t1 = TickCount::now();
    TimeInterval interval;
    //----------------------------------------------------------------------
    //	Prepare the simulation with cell linked list, configuration
    //	and case specified initial condition if necessary.
    //----------------------------------------------------------------------
    sph_system.initializeSystemCellLinkedLists();
    sph_system.initializeSystemConfigurations();
    wall_boundary_normal_direction.exec();
    //----------------------------------------------------------------------
    //	First output before the main loop.
    //----------------------------------------------------------------------
    write_real_body_states.writeToFile(number_of_iterations);
    wave_gauge.writeToFile(number_of_iterations);
    //----------------------------------------------------------------------
    //	Main loop of time stepping starts here.
    //----------------------------------------------------------------------
    while (GlobalStaticVariables::physical_time_ < end_time)
    {
        Real integral_time = 0.0;
        while (integral_time < output_interval)
        {
            initialize_time_step_to_fluid.exec();

            Real Dt = get_fluid_advection_time_step_size.exec();
            // free_stream_surface_indicator.exec();
            update_density_by_summation.exec();
            viscous_acceleration.exec();
            // transport_velocity_correction.exec();

            Real relaxation_time = 0.0;
            while (relaxation_time < Dt)
            {
                dt = get_fluid_time_step_size.exec();
                pressure_relaxation.exec(dt);
                density_relaxation.exec(dt);

                relaxation_time += dt;
                integral_time += dt;
                total_time += dt;
                if (total_time >= relax_time)
                    GlobalStaticVariables::physical_time_ += dt;
            }

            if (number_of_iterations % screen_output_interval == 0)
            {
                std::cout << std::fixed << std::setprecision(9) << "N=" << number_of_iterations
                          << "	Total Time = " << total_time
                          << "	Physical Time = " << GlobalStaticVariables::physical_time_
                          << "	Dt = " << Dt << "	dt = " << dt << "\n";
            }
            number_of_iterations++;
            water_block.updateCellLinkedListWithParticleSort(100);
            wall_boundary.updateCellLinkedList();
            water_block_complex.updateConfiguration();

            if (total_time >= relax_time)
            {
                wave_gauge.writeToFile(number_of_iterations);
            }
        }

        TickCount t2 = TickCount::now();
        if (total_time >= relax_time)
            write_real_body_states.writeToFile();
        TickCount t3 = TickCount::now();
        interval += t3 - t2;
    }

    TickCount t4 = TickCount::now();

    TimeInterval tt;
    tt = t4 - t1 - interval;
    std::cout << "Total wall time for computation: " << tt.seconds() << " seconds." << std::endl;

    if (sph_system.GenerateRegressionData())
    {
        wave_gauge.generateDataBase(1e-3);
    }
    else
    {
        wave_gauge.testResult();
    }

    return 0;
}
