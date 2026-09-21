local entries = 78;
{
  driver: {
    cpp: 'generate_layers',
    layers: {
      spill: { parent: 'job', total: entries},
    },
  },

  sources: {
    rntuple_source: {
      cpp: 'read_sim_file',
      input_file:  '../aegir/fixed_target_output.root',
      ntuple_name: 'events',
      layer:       'spill',
      entries: entries,
      pot : 1000
     },
  },

  modules: {
     digitise_hits: {
         cpp: 'digitise_hits',
         layer: 'spill',
     },
    output: {
      cpp: 'digitised_output_module',
      rntuple_file: 'digitised_hits_time.root',
    },
  },
}
