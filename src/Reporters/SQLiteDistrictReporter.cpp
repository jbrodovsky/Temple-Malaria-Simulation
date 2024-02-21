#include "SQLiteDistrictReporter.h"

#include <fmt/printf.h>

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
void SQLiteDistrictReporter::initialize(int jobNumber, std::string path) {
  // Inform the user of the reporter type and make sure there are districts
  VLOG(1) << "Using SQLiteDbReporterwith aggregation at the district level.";
  if (!SpatialData::get_instance().has_raster(SpatialData::Districts)) {
    LOG(ERROR) << "District raster must be present when aggregating data at "
                  "the district level.";
    throw std::invalid_argument("No district raster present");
  }

  SQLiteDbReporter::initialize(jobNumber, path);
}
// Collect and store monthly site data
// Aggregates data related to various site metrics and stores them in the
// database
void SQLiteDistrictReporter::monthly_site_data(int monthId) {
  TransactionGuard transaction{db.get()};
  auto &districtLookup = SpatialData::get_instance().district_lookup();

  // Prepare the data structures
  auto &ageClasses = Model::CONFIG->age_structure();
  auto districts = SpatialData::get_instance().get_district_count();
  std::vector<double> eir(districts, 0);
  std::vector<double> pfprUnder5(districts, 0);
  std::vector<double> pfpr2to10(districts, 0);
  std::vector<double> pfprAll(districts, 0);
  std::vector<int> population(districts, 0);
  std::vector<int> clinicalEpisodes(districts, 0);
  std::vector<std::vector<int>> clinicalEpisodesByAgeClass(
      districts, std::vector<int>(ageClasses.size(), 0));
  std::vector<int> treatments(districts, 0);
  std::vector<int> treatmentFailures(districts, 0);
  std::vector<int> nontreatment(districts, 0);
  std::vector<int> treatmentsUnder5(districts, 0);
  std::vector<int> treatmentsOver5(districts, 0);

  auto* index =
      Model::POPULATION->get_person_index<PersonIndexByLocationStateAgeClass>();
  std::vector<int> infectionsDistrict(districts, 0);

  for (auto location = 0; location < index->vPerson().size(); location++) {
    for (auto hs = 0; hs < Person::NUMBER_OF_STATE - 1; hs++) {
      for (unsigned int ac = 0; ac < ageClasses.size(); ac++) {
        for (auto &person : index->vPerson()[location][hs][ac]) {
          // Is the individual infected by at least one parasite?
          if (person->all_clonal_parasite_populations()->parasites()->empty()) {
            continue;
          }

          // Calculate the correct index and update the count
          auto district = districtLookup[location];
          infectionsDistrict[district]++;
        }
      }
    }
  }
  // Collect the data
  for (auto location = 0; location < Model::CONFIG->number_of_locations();
       location++) {
    // If the population is zero, press on
    auto locationPopulation =
        static_cast<int>(Model::POPULATION->size(location));
    if (locationPopulation == 0) { continue; }

    auto district = districtLookup[location];

    // Collect the simple data
    population[district] += locationPopulation;
    clinicalEpisodes[district] +=
        Model::MAIN_DATA_COLLECTOR
            ->monthly_number_of_clinical_episode_by_location()[location];

    treatments[district] +=
        Model::MAIN_DATA_COLLECTOR
            ->monthly_number_of_treatment_by_location()[location];
    treatmentFailures[district] +=
        Model::MAIN_DATA_COLLECTOR
            ->monthly_treatment_failure_by_location()[location];
    nontreatment[district] +=
        Model::MAIN_DATA_COLLECTOR
            ->monthly_nontreatment_by_location()[location];

    for (auto ndx = 0; ndx < ageClasses.size(); ndx++) {
      // Collect the treatment by age class, following the 0-59 month convention
      // for under-5
      if (ageClasses[ndx] < 5) {
        treatmentsUnder5[district] +=
            Model::MAIN_DATA_COLLECTOR
                ->monthly_number_of_treatment_by_location_age_class()[location]
                                                                     [ndx];
      } else {
        treatmentsOver5[district] +=
            Model::MAIN_DATA_COLLECTOR
                ->monthly_number_of_treatment_by_location_age_class()[location]
                                                                     [ndx];
      }

      // collect the clinical episodes by age class
      clinicalEpisodesByAgeClass[district][ndx] +=
          Model::MAIN_DATA_COLLECTOR
              ->monthly_number_of_clinical_episode_by_location_age_class()
                  [location][ndx];
    }

    // EIR and PfPR is a bit more complicated since it could be an invalid value
    // early in the simulation, and when aggregating at the district level the
    // weighted mean needs to be reported instead
    if (Model::MAIN_DATA_COLLECTOR->recording_data()) {
      auto eirLocation =
          Model::MAIN_DATA_COLLECTOR->EIR_by_location_year()[location].empty()
              ? 0
              : Model::MAIN_DATA_COLLECTOR->EIR_by_location_year()[location]
                    .back();
      eir[district] += (eirLocation * locationPopulation);
      pfprUnder5[district] +=
          (Model::MAIN_DATA_COLLECTOR->get_blood_slide_prevalence(location, 0,
                                                                  5)
           * locationPopulation);
      pfpr2to10[district] +=
          (Model::MAIN_DATA_COLLECTOR->get_blood_slide_prevalence(location, 2,
                                                                  10)
           * locationPopulation);
      pfprAll[district] += (Model::MAIN_DATA_COLLECTOR
                                ->blood_slide_prevalence_by_location()[location]
                            * locationPopulation);
    }
  }

  std::vector<std::string> values;
  for (auto district = 0; district < districts; district++) {
    double calculatedEir =
        (eir[district] != 0) ? (eir[district] / population[district]) : 0;
    double calculatedPfprUnder5 =
        (pfprUnder5[district] != 0)
            ? (pfprUnder5[district] / population[district]) * 100.0
            : 0;
    double calculatedPfpr2to10 =
        (pfpr2to10[district] != 0)
            ? (pfpr2to10[district] / population[district]) * 100.0
            : 0;
    double calculatedPfprAll =
        (pfprAll[district] != 0)
            ? (pfprAll[district] / population[district]) * 100.0
            : 0;

    std::string singleRow = fmt::format(
        "({}, {}, {}, {}", monthId,
        SpatialData::get_instance().adjust_simulation_district_to_raster_index(
            district),
        population[district], clinicalEpisodes[district]);

    // Append clinical episodes by age class
    for (const auto &episodes : clinicalEpisodesByAgeClass[district]) {
      singleRow += fmt::format(", {}", episodes);
    }

    singleRow += fmt::format(
        ", {}, {}, {}, {}, {}, {}, {}, {}, {}, {})", treatments[district],
        calculatedEir, calculatedPfprUnder5, calculatedPfpr2to10,
        calculatedPfprAll, infectionsDistrict[district],
        treatmentFailures[district], nontreatment[district],
        treatmentsUnder5[district], treatmentsOver5[district]);

    values.push_back(singleRow);
  }

  std::string query =
      INSERT_SITE_PREFIX + StringHelpers::join(values, ",") + ";";
  db->execute(query);
}

// Collect and store monthly genome data
// Aggregates and stores data related to the genotypes found in the population
void SQLiteDistrictReporter::monthly_genome_data(int monthId) {
  TransactionGuard transaction{db.get()};
  auto &districtLookup = SpatialData::get_instance().district_lookup();

  // Cache some values
  auto genotypes = Model::CONFIG->number_of_parasite_types();
  auto districts = SpatialData::get_instance().get_district_count();
  auto* index =
      Model::POPULATION->get_person_index<PersonIndexByLocationStateAgeClass>();
  auto ageClasses = index->vPerson()[0][0].size();

  // Prepare the data structures
  std::vector<int> individual(genotypes, 0);
  std::vector<int> infectionsDistrict(districts, 0);
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
    auto district = districtLookup[location];
    int infectedIndividuals = 0;

    for (auto hs = 0; hs < Person::NUMBER_OF_STATE - 1; hs++) {
      // Iterate over all the age classes
      for (unsigned int ac = 0; ac < ageClasses; ac++) {
        // Iterate over all the genotypes
        auto peopleInAgeClass = index->vPerson()[location][hs][ac];
        for (auto &person : peopleInAgeClass) {
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
            auto* parasitePopulation = (*parasites)[ndx];
            auto genotypeId = parasitePopulation->genotype()->genotype_id();
            occurrences[district][genotypeId]++;
            occurrencesZeroToFive[district][genotypeId] += (age <= 5) ? 1 : 0;
            occurrencesTwoToTen[district][genotypeId] +=
                (age >= 2 && age <= 10) ? 1 : 0;
            individual[genotypeId]++;

            // Count a clinical occurrence if the individual has clinical
            // symptoms
            clinicalOccurrences[district][genotypeId] += clinical;
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
    infectionsDistrict[district] += infectedIndividuals;
  }

  std::vector<std::string> insertValues;
  // Iterate over the districts and append the query
  std::string insertGenotypes;
  std::string updateInfections;
  for (auto district = 0; district < districts; district++) {
    for (auto genotype = 0; genotype < genotypes; genotype++) {
      if (weightedOccurrences[district][genotype] == 0) { continue; }
      std::string singleRow =
          fmt::format("({}, {}, {}, {}, {}, {}, {}, {})", monthId,
                      SpatialData::get_instance()
                          .adjust_simulation_district_to_raster_index(district),
                      genotype, occurrences[district][genotype],
                      clinicalOccurrences[district][genotype],
                      occurrencesZeroToFive[district][genotype],
                      occurrencesTwoToTen[district][genotype],
                      weightedOccurrences[district][genotype]);

      insertValues.push_back(singleRow);
    }
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

