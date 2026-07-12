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
      cpp: 'form_module',
      technology: 'ROOT_TTREE',
      products: [
        'ubt_hits',
        'sbt_hits',
        'straw_tubes_hits',
        'calorimeter_hits',
        'timing_detector_hits',
      ],
    },
  },
}
