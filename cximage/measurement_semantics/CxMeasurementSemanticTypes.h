#ifndef CXIMAGE_MEASUREMENT_SEMANTICS_CXMEASUREMENTSEMANTICTYPES_H
#define CXIMAGE_MEASUREMENT_SEMANTICS_CXMEASUREMENTSEMANTICTYPES_H

#include <map>
#include <string>
#include <vector>

struct CxMeasurementBehaviorStep
{
    int sequence = 0;
    std::string operation;
    std::string tool;
    std::string object_ref;
    std::string status;
    std::string reason;
    std::map<std::string, double> numeric_metrics;
};

struct CxMeasurementObservation
{
    std::string observation_id;
    std::string case_id;
    std::string entity_ref;
    std::string tool;
    std::string quantity;
    double value = 0.0;
    std::string unit = "px";
    std::string source;
    std::string status;
};

struct CxMeasurementRelation
{
    std::string relation_id;
    std::string subject_ref;
    std::string predicate;
    std::string object_ref;
    double value = 0.0;
    std::string unit = "px";
    std::string source;
    std::string status;
};

struct CxMeasurementFeatureValue
{
    std::string name;
    double value = 0.0;
    std::string source_observation_ref;
};

struct CxMeasurementFeatureVector
{
    std::string feature_vector_id;
    std::string case_id;
    std::string object_ref;
    std::vector<CxMeasurementFeatureValue> values;
    std::string status;
};

struct CxMeasurementSemanticPackageRef
{
    std::string schema = "cxvision.measurement_semantic_package_ref.v1";
    std::string status;
    std::string package_dir;
    std::string result_ref;
    std::string evidence_ref;
};

#endif
