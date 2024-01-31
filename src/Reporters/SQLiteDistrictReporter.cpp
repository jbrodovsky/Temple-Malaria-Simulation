#include "SQLiteDistrictReporter.h"

#include "Core/Config/Config.h"
#include "GIS/SpatialData.h"
#include "Helpers/StringHelpers.h"
#include "MDC/MainDataCollector.h"
#include "Model.h"
#include "Population/Population.h"
#include "Population/Properties/PersonIndexByLocationStateAgeClass.h"
#include "easylogging++.h"

// Initialize the reporter
// Sets up the database and prepares it for data entry
void SQLiteDistrictReporter::initialize(int job_number, std::string path) {
  // Inform the user of the reporter type and make sure there are districts
  VLOG(1) << "Using SQLiteDbReporterwith aggregation at the district level.";
  if (!SpatialData::get_instance().has_raster(SpatialData::Districts)) {
    LOG(ERROR) << "District raster must be present when aggregating data at "
                  "the district level.";
    throw std::invalid_argument("No district raster present");
  }

  SQLiteDbReporter::initialize(job_number, path);
}
// Collect and store monthly site data
// Aggregates data related to various site metrics and stores them in the
// database
void SQLiteDistrictReporter::monthly_site_data(int month_id) {
  auto &district_lookup = SpatialData::get_instance().district_lookup();

  // Prepare the data structures
  auto age_classes = Model::CONFIG->age_structure();
  auto districts = SpatialData::get_instance().get_district_count();
  auto first_index = SpatialData::get_instance().get_first_district();
  std::vector<double> eir(districts, 0);
  std::vector<double> pfpr_under5(districts, 0);
  std::vector<double> pfpr_2to10(districts, 0);
  std::vector<double> pfpr_all(districts, 0);
  std::vector<int> population(districts, 0);
  std::vector<int> clinical_episodes(districts, 0);
  std::vector<int> treatments(districts, 0);
  std::vector<int> treatment_failures(districts, 0);
  std::vector<int> nontreatment(districts, 0);
  std::vector<int> treatments_under5(districts, 0);
  std::vector<int> treatments_over5(districts, 0);

  // Collect the data
  for (auto location = 0; location < Model::CONFIG->number_of_locations();
       location++) {
    // If the population is zero, press on
    auto location_population =
        static_cast<int>(Model::POPULATION->size(location));
    if (location_population == 0) { continue; }

    // Note the district we are in, make sure things are zero indexed
    auto district = district_lookup[location] - first_index;

    // Collect the simple data
    population[district] += location_population;
    clinical_episodes[district] +=
        Model::MAIN_DATA_COLLECTOR
            ->monthly_number_of_clinical_episode_by_location()[location];
    treatments[district] +=
        Model::MAIN_DATA_COLLECTOR
            ->monthly_number_of_treatment_by_location()[location];
    treatment_failures[district] +=
        Model::MAIN_DATA_COLLECTOR
            ->monthly_treatment_failure_by_location()[location];
    nontreatment[district] +=
        Model::MAIN_DATA_COLLECTOR
            ->monthly_nontreatment_by_location()[location];

    // Collect the treatment by age class, following the 0-59 month convention
    // for under-5
    for (auto ndx = 0; ndx < age_classes.size(); ndx++) {
      if (age_classes[ndx] < 5) {
        treatments_under5[district] +=
            Model::MAIN_DATA_COLLECTOR
                ->monthly_number_of_treatment_by_location_age_class()[location]
                                                                     [ndx];
      } else {
        treatments_over5[district] +=
            Model::MAIN_DATA_COLLECTOR
                ->monthly_number_of_treatment_by_location_age_class()[location]
                                                                     [ndx];
      }
    }

    // EIR and PfPR is a bit more complicated since it could be an invalid value
    // early in the simulation, and when aggregating at the district level the
    // weighted mean needs to be reported instead
    if (Model::MAIN_DATA_COLLECTOR->recording_data()) {
      auto eir_location =
          Model::MAIN_DATA_COLLECTOR->EIR_by_location_year()[location].empty()
              ? 0
              : Model::MAIN_DATA_COLLECTOR->EIR_by_location_year()[location]
                    .back();
      eir[district] += (eir_location * location_population);
      pfpr_under5[district] +=
          (Model::MAIN_DATA_COLLECTOR->get_blood_slide_prevalence(location, 0,
                                                                  5)
           * location_population);
      pfpr_2to10[district] +=
          (Model::MAIN_DATA_COLLECTOR->get_blood_slide_prevalence(location, 2,
                                                                  10)
           * location_population);
      pfpr_all[district] +=
          (Model::MAIN_DATA_COLLECTOR
               ->blood_slide_prevalence_by_location()[location]
           * location_population);
    }
  }

  std::vector<std::string> values;

  for (auto district = 0; district < districts; district++) {
    double calculatedEir =
        (eir[district] != 0) ? (eir[district] / population[district]) : 0;
    double calculatedPfprUnder5 =
        (pfpr_under5[district] != 0)
            ? (pfpr_under5[district] / population[district]) * 100.0
            : 0;
    double calculatedPfpr2to10 =
        (pfpr_2to10[district] != 0)
            ? (pfpr_2to10[district] / population[district]) * 100.0
            : 0;
    double calculatedPfprAll =
        (pfpr_all[district] != 0)
            ? (pfpr_all[district] / population[district]) * 100.0
            : 0;

    std::string singleRow = fmt::format(
        "({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})", month_id,
        district + first_index, population[district],
        clinical_episodes[district], treatments[district], calculatedEir,
        calculatedPfprUnder5, calculatedPfpr2to10, calculatedPfprAll,
        treatment_failures[district], nontreatment[district],
        treatments_under5[district], treatments_over5[district]);
    values.push_back(singleRow);
  }

  std::string query =
      INSERT_SITE_PREFIX + StringHelpers::join(values, ",") + ";";
  db->execute(query);
}

// Collect and store monthly genome data
// Aggregates and stores data related to the genotypes found in the population
void SQLiteDistrictReporter::monthly_genome_data(int month_id) {
  auto &district_lookup = SpatialData::get_instance().district_lookup();

  // Cache some values
  auto genotypes = Model::CONFIG->number_of_parasite_types();
  auto districts = SpatialData::get_instance().get_district_count();
  auto first_index = SpatialData::get_instance().get_first_district();
  auto* index =
      Model::POPULATION->get_person_index<PersonIndexByLocationStateAgeClass>();
  auto age_classes = index->vPerson()[0][0].size();

  // Prepare the data structures
  std::vector<int> individual(genotypes, 0);
  std::vector<int> infections_district(districts, 0);
  std::vector<std::vector<int>> occurrences(districts,
                                            std::vector<int>(genotypes, 0));
  std::vector<std::vector<int>> clinicalOccurrences(
      districts, std::vector<int>(genotypes, 0));
  std::vector<std::vector<int>> occurrencesZeroToFive(
      districts, std::vector<int>(genotypes, 0));
  std::vector<std::vector<int>> occurrencesTwoToTen(
      districts, std::vector<int>(genotypes, 0));
  std::vector<std::vector<double>> weightedOccurrences(
      districts, std::vector<double>(genotypes, 0));

  // Iterate over all the possible states
  for (auto location = 0; location < index->vPerson().size(); location++) {
    // Get the current index and apply the off set, so we are zero aligned
    auto district = district_lookup[location] - first_index;
    int infectedIndividuals = 0;

    for (auto hs = 0; hs < Person::NUMBER_OF_STATE - 1; hs++) {
      // Iterate over all the age classes
      for (unsigned int ac = 0; ac < age_classes; ac++) {
        // Iterate over all the genotypes
        auto age_class = index->vPerson()[location][hs][ac];
        for (auto &person : age_class) {
          // Get the person, press on if they are not infected
          auto* parasites =
              person->all_clonal_parasite_populations()->parasites();
          auto size = parasites->size();
          if (size == 0) { continue; }

          // Note the age and clinical status of the person
          auto age = person->age();
          auto clinical = static_cast<int>(person->host_state()
                                           == Person::HostStates::CLINICAL);

          // Update the count of infected individuals
          infectedIndividuals++;

          // Count the genotypes present in the individual
          for (unsigned int ndx = 0; ndx < size; ndx++) {
            auto* parasite_population = (*parasites)[ndx];
            auto genotype_id = parasite_population->genotype()->genotype_id();
            occurrences[district][genotype_id]++;
            occurrencesZeroToFive[district][genotype_id] += (age <= 5);
            occurrencesTwoToTen[district][genotype_id] +=
                (age >= 2 && age <= 10);
            individual[genotype_id]++;

            // Count a clinical occurrence if the individual has clinical
            // symptoms
            clinicalOccurrences[district][genotype_id] += clinical;
          }

          // Update the weighted occurrences and reset the individual count
          for (unsigned int ndx = 0; ndx < genotypes; ndx++) {
            weightedOccurrences[district][ndx] +=
                (individual[ndx] / static_cast<double>(size));
            individual[ndx] = 0;
          }
        }
      }
    }

    // Update the number of individuals in the location
    infections_district[district] += infectedIndividuals;
  }

  std::vector<std::string> insertValues;

  // Iterate over the districts and append the query
  std::string insert_genotypes;
  std::string update_infections;
  for (auto district = 0; district < districts; district++) {
    for (auto genotype = 0; genotype < genotypes; genotype++) {
      if (weightedOccurrences[district][genotype] == 0) { continue; }
      std::string singleRow = fmt::format(
          "({}, {}, {}, {}, {}, {}, {}, {})", month_id, district + first_index,
          genotype, occurrences[district][genotype],
          clinicalOccurrences[district][genotype],
          occurrencesZeroToFive[district][genotype],
          occurrencesTwoToTen[district][genotype],
          weightedOccurrences[district][genotype]);

      insertValues.push_back(singleRow);
    }

    std::string updateQuery =
        fmt::format(UPDATE_INFECTED_INDIVIDUALS, infections_district[district],
                    month_id, district + first_index);
    db->execute(updateQuery);
  }
  if (insertValues.empty()) {
    LOG(INFO) << "No genotypes recorded in the simulation at timestep, "
              << Model::SCHEDULER->current_time();
    return;
  }

  std::string insertQuery =
      INSERT_GENOTYPE_PREFIX + StringHelpers::join(insertValues, ",") + ";";
  db->execute(insertQuery);
}

// Update the count of infected individuals in the database
// Aggregates data on the number of infected individuals per district and
// updates the database
void SQLiteDistrictReporter::monthly_infected_individuals(int month_id) {
  auto &district_lookup = SpatialData::get_instance().district_lookup();
  // Cache some values and prepare the data structure
  auto districts = SpatialData::get_instance().get_district_count();
  auto first_index = SpatialData::get_instance().get_first_district();
  auto* index =
      Model::POPULATION->get_person_index<PersonIndexByLocationStateAgeClass>();
  auto age_classes = index->vPerson()[0][0].size();
  std::vector<int> infections_district(districts, 0);

  // Iterate overall the possible locations and states
  for (auto location = 0; location < index->vPerson().size(); location++) {
    for (auto hs = 0; hs < Person::NUMBER_OF_STATE - 1; hs++) {
      for (unsigned int ac = 0; ac < age_classes; ac++) {
        for (auto &person : index->vPerson()[location][hs][ac]) {
          // Is the individual infected by at least one parasite?
          if (person->all_clonal_parasite_populations()->parasites()->empty()) {
            continue;
          }

          // Calculate the correct index and update the count
          auto district = district_lookup[location] - first_index;
          infections_district[district]++;
        }
      }
    }
  }
  // Iterate over the districts and append the query
  for (auto district = 0; district < districts; district++) {
    std::string updateQuery =
        fmt::format(UPDATE_INFECTED_INDIVIDUALS, infections_district[district],
                    month_id, district + first_index);
    db->execute(updateQuery);
  }
}
