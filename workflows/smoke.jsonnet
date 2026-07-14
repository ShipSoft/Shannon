{
  driver: {
    cpp: 'generate_layers',
    layers: {
      spill: { parent: 'job', total: 10 },
    },
  },

  sources: {
    rntuple_source: {
      cpp: 'read_sim_file',
      input_file: 'smoke_input.root',
      ntuple_name: 'events',
      layer: 'spill',
    },
  },

  modules: {
    digitise_hits: {
      cpp: 'digitise_hits',
      layer: 'spill',
      seed: 42,
    },
    digitised_output: {
      cpp: 'digitised_output_module',
      mode: 'digi',
      rntuple_file: 'smoke_digi_output.root',
      histo_file: 'smoke_digi_validation.root',
      creator: 'digitise_hits',
      layer: 'spill',
    },
  },
}
