#include "caffe/util/insert_conversions.hpp"

namespace caffe {

void InsertConversions(const NetParameter& param, NetParameter* param_convert) {
  // Initialize by copying from the input NetParameter.
  param_convert->CopyFrom(param);
  param_convert->clear_layer();
  map<string, DataType> blob_data_types;
  map<string, string> blob_name_to_layer_name;
  for (int_tp i = 0; i < param.layer_size(); ++i) {
    const LayerParameter& layer_param = param.layer(i);
    LayerParameter* copy_layer_param = param_convert->add_layer();
    copy_layer_param->CopyFrom(param.layer(i));
    for (int_tp j = 0; j < layer_param.bottom_size(); ++j) {
      const string& blob_name = layer_param.bottom(j);
      if (blob_data_types.find(blob_name) ==
          blob_data_types.end()) {
        LOG(FATAL) << "Unknown bottom blob '" << blob_name << "' (layer '"
                   << layer_param.name() << "', bottom index " << j << ")";
      } else {
        if (layer_param.bottom_data_type() != blob_data_types[blob_name]) {
          string convert_blob_name = ConversionBlobName(
              blob_name_to_layer_name[blob_name], blob_name, j);
          LayerParameter* convert_layer_param = param_convert->add_layer();
          const float loss_weight = layer_param.loss_weight(j);
          ConfigureConversionLayer(blob_name_to_layer_name[blob_name],
                                   blob_name, j, loss_weight,
                                   convert_layer_param,
                                   blob_data_types[blob_name],
                                   layer_param.bottom_data_type(),
                                   layer_param.quantizer_index());
          copy_layer_param->set_bottom(j, convert_blob_name);
        }
      }
    }
    for (int_tp j = 0; j < layer_param.top_size(); ++j) {
      const string& blob_name = layer_param.top(j);
      blob_data_types[blob_name] = layer_param.top_data_type();
      blob_name_to_layer_name[blob_name] = layer_param.name();
    }
  }
}

void ConfigureConversionLayer(const string& layer_name, const string& blob_name,
    const int_tp blob_idx, const float loss_weight,
    LayerParameter* convert_layer_param, DataType bottom_data_type,
    DataType top_data_type, size_t quantizer_index) {
  convert_layer_param->Clear();
  convert_layer_param->add_bottom(blob_name);
  convert_layer_param->set_name(ConversionLayerName(layer_name, blob_name,
                                                    blob_idx));
  convert_layer_param->set_type("Quantizer");
  convert_layer_param->set_compute_data_type(bottom_data_type);
  convert_layer_param->set_top_data_type(top_data_type);
  convert_layer_param->set_bottom_data_type(bottom_data_type);
  convert_layer_param->set_quantizer_index(quantizer_index);
  convert_layer_param->add_top(ConversionBlobName(layer_name, blob_name,
                                                  blob_idx));
  convert_layer_param->add_loss_weight(loss_weight);
}

string ConversionLayerName(const string& layer_name, const string& blob_name,
    const int_tp blob_idx) {
  ostringstream convert_layer_name;
  convert_layer_name << blob_name << "_" << layer_name << "_" << blob_idx
      << "_converted";
  return convert_layer_name.str();
}

string ConversionBlobName(const string& layer_name, const string& blob_name,
    const int_tp blob_idx) {
  ostringstream convert_blob_name;
  convert_blob_name << blob_name << "_" << layer_name << "_" << blob_idx
      << "_converted";
  return convert_blob_name.str();
}

}
