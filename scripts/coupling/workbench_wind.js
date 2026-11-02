/* Optional prescribed-wind controls on the existing saved-run workbench. */
window.FireFluidWind=(()=>{
  let presets={},previousWind=false;
  const $=id=>document.getElementById(id);
  const number=v=>Number.isFinite(v)?`${v>=0?'+':''}${v.toFixed(3)}`:'—';
  function isWind(p){return Object.hasOwn(presets,p)}
  function setValues(values){for(const [key,value] of Object.entries(values)){const element=$(key);if(element)element.value=String(value)}}
  function template(){
    const p=$('profile').value,on=isWind(p);
    $('wind_controls').style.display=on?'flex':'none';$('legacy_wind').style.display=on?'none':'flex';
    $('legacyDescription').style.display=on?'none':'block';$('windDescription').style.display=on?'block':'none';
    $('windDescription').textContent=on?presets[p].description+' Native steps: Fire 0.02 s, gas 0.005 s. Live grid: 8³ gas and 17² Fire. Gust response remains resolution-sensitive; spatial accuracy is unqualified.':'';
    $('duration_s').max=on?'6.4':'3.2';for(const option of $('exchange_s').options)if(option.hasAttribute('data-wind-only')){option.disabled=!on;option.hidden=!on}
    for(const id of ['gust_start_control','gust_stop_control'])$(id).style.display=on&&['abrupt_gust','ramped_gust','single_gust_recovery','repeated_gust'].includes(p)?'flex':'none';
    $('ramp_control').style.display=on&&p==='ramped_gust'?'flex':'none';previousWind=on;
  }
  function changed(){
    const p=$('profile').value;
    if(isWind(p))setValues(presets[p].defaults);
    else if(previousWind && ['baseline','warm_plume'].includes(p))setValues(p==='baseline'?{feedback:true,wind_scale:20,source_scale:1,sample_height_m:.0625,exchange_s:.1,duration_s:2}:{feedback:true,wind_scale:1,source_scale:3,sample_height_m:.0625,exchange_s:.1,duration_s:3.2});
    template();
  }
  return {
    metadata(value){presets=value||{};for(const [id,entry] of Object.entries(presets))if(!Array.from($('profile').options).some(o=>o.value===id)){const o=document.createElement('option');o.value=id;o.textContent=entry.label;$('profile').append(o)}},
    template,changed,
    settings(base){if(isWind(base.profile)){delete base.wind_scale;for(const id of ['inlet_speed_m_s','gust_start_s','gust_stop_s','ramp_s'])base[id]=Number($(id).value)}return base},
    frame(f){const node=$('windStatus');if(!f.wind_control){node.style.display='none';return}node.style.display='block';const samples=f.feedback?.tangent_velocity_m_s||[];const applied=samples.length?samples.reduce((sum,p)=>sum+p[0],0)/samples.length:0;node.textContent=`Inlet last / next: ${number(f.wind_control.last_target_velocity_x_m_s)} / ${number(f.wind_control.next_target_velocity_x_m_s)} m/s · Mean sampled Fire input: ${number(applied)} m/s · Prescribed inlet volume: ${f.wind_control.inlet_volume_m3.toFixed(4)} m³`;}
  };
})();
