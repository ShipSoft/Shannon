{
  driver: {
    cpp: 'generate_layers',
    layers: {
      spill: { parent: 'job', total: 4},
    },
  },

  sources: {
    rntuple_source: {
      cpp: 'read_sim_file',
      input_file:  '../aegir/fixed_target_mt_output.root',
      ntuple_name: 'events',
      field_name:  'sim_particles',
      layer:       'spill'
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
