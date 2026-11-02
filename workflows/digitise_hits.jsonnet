local input_file = '../aegir/fixed_target_mt_output.root';
local ntuple_name = 'events';
{
  stage: 'digitisation',
  driver: {
    cpp: 'rntuple_entries_driver',
    input_file: input_file,
    ntuple_name: ntuple_name,
    layer: 'spill',
    // max_events: 100,  // uncomment for a quick partial run
  },

  sources: {
    rntuple_source: {
      cpp: 'read_sim_file',
      input_file:  input_file,
      ntuple_name: ntuple_name,
      layer:       'spill',
     },
  },

  modules: {
     digitise_hits: {
         cpp: 'digitise_hits',
         layer: 'spill',
     },
    output: {
      cpp: 'digitised_output_module',
      rntuple_file: 'digitised_hits.root',
    },
  },
}
