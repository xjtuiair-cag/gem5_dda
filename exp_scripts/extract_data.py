#! /bin/env python3

import os
import pandas as pd

from string import Template

def extract_stats(record_list, path_template, stats_names):
    filled_record_list = []
    for record in record_list:
        # apply record values to path template
        stats_path = path_template.substitute(record) + '/stats.txt' 
        filled_record = dict(record)

        if not os.path.exists(stats_path):
            print(stats_path)
            for stats_name in stats_names:
                filled_record[stats_name] = 'NAN'
        else: 
            for stats_name in stats_names:
                filled_record[stats_name] = 'EMPTY'
                with open(stats_path, 'r') as stats:
                    for line in stats:
                        # remove comments
                        line = line.split('#')[0].strip()
                        # only for one data per metric-line
                        # only record the first reaching stats_name
                        if stats_name in line:
                            filled_record[stats_name] = line.split(stats_name)[1].strip()
                            break
        filled_record_list.append(filled_record)
    return filled_record_list

def gen_field_record(field_values):
    pre_record_list = []
    cur_record_list = []
    for cur_field_name, cur_field_values in field_values.items():
        for value in cur_field_values:
            if not isinstance(value, str):
                value = str(value)
            if len(pre_record_list) == 0:
                cur_record_list.append({cur_field_name: value})
            else:
                for record in pre_record_list: 
                    new_record = dict(record)
                    new_record[cur_field_name] = value
                    cur_record_list.append(new_record)

        pre_record_list = cur_record_list
        cur_record_list = []
    return pre_record_list


### script config start here ###

#stats_names = [
#        'simSeconds',
#        'system.cpu.dcache.prefetcher.accuracyPerPC::400c7c ',
#        'system.cpu.dcache.prefetcher.timely_accuracy_perPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.coveragePerPC::400c7c ',
#        'system.cpu.dcache.prefetcher.dmp_pfIdentifiedPerPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.pfBufferHitPerPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.pfRemovedDemandPerPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.pfTransFailedPerPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.pfIssuedPerPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.pfLatePerPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.pfUsefulPerPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.pfUnusedPerPfPC::400c7c ',
#        'system.cpu.dcache.prefetcher.demandMshrMissesPerPC::400c7c ',
#        ]

#stats_names = [
#        'simSeconds',
#        'system.l2.prefetcher.accuracyPerPC::400ca0 ',
#        'system.l2.prefetcher.timely_accuracy_perPfPC::400ca0 ',
#        'system.l2.prefetcher.coveragePerPC::400ca0 ',
#        'system.l2.prefetcher.dmp_pfIdentifiedPerPfPC::400ca0 ',
#        'system.l2.prefetcher.pfBufferHitPerPfPC::400ca0 ',
#        'system.l2.prefetcher.pfRemovedDemandPerPfPC::400ca0 ',
#        'system.l2.prefetcher.pfTransFailedPerPfPC::400ca0 ',
#        'system.l2.prefetcher.pfIssuedPerPfPC::400ca0 ',
#        'system.l2.prefetcher.pfLatePerPfPC::400ca0 ',
#        'system.l2.prefetcher.pfUsefulPerPfPC::400ca0 ',
#        'system.l2.prefetcher.pfUnusedPerPfPC::400ca0 ',
#        'system.l2.prefetcher.demandMshrMissesPerPC::400ca0 ',
#        'system.l2.prefetcher.pfLateRatePerPfPC::400ca0',
#        ]

stats_names = [
        'simSeconds',
        'system.l2.prefetcher.pf_cosumed_perPfPC::400ca0',
        'system.l2.prefetcher.pf_effective_perPfPC::400ca0',
        'system.l2.prefetcher.pf_timely_perPfPC::400ca0',
        'system.l2.prefetcher.accuracy_cache_perPfPC::400ca0',
        'system.l2.prefetcher.accuracy_prefetcher_perPfPC::400ca0',
        'pfLateRatePerPfPC::400ca0',
        ]

#field_values = {
#        'bench' : ['spmv'], 
#        'matrix' : ['mid'], 
#        'threads' : [4, 6, 8, 12, 16], 
#        'prefetcher' : ['stride', 'dmp'], 
#        'degree' : [4], 
#        'DRAM' : ['DDR4'],
#        }
#path_template = Template('${bench}_${matrix}_TH${threads}/${prefetcher}_DG${degree}_${DRAM}_warm_m5out')

field_values = {
        'bench' : ['bfs'], 
        'matrix' : ['sx-superuser'],
        #'threads' : [4, 6, 8, 12, 16], 
        'prefetcher' : ['strideL1_dmpL2'], 
        'stride_degree' : [1, 2, 4, 6, 8], 
        'range_degree' : [2, 4, 8, 12, 16], 
        'range_ahead' : [0, 1, 2, 4, 8], 
        #'dcache_size' : [16, 32, 64, 128, 256],
        'l2cache_size' : [64, 128, 256, 512, 1024],
        'DRAM' : ['Simple'],
        #'level': ['full']
        }
path_template = Template(
        'exp_workspace_strideL1_dmpL2_lat-3-13/' \
        '${bench}_${matrix}/' \
        #'${prefetcher}_' \
        'SDG${stride_degree}_' \
        'RAH${range_ahead}_' \
        'RDG${range_degree}_' \
        #'DCache${dcache_size}kB_' \
        'L2Cache${l2cache_size}kB_' \
        '${DRAM}_' \
        'BigPFQ_double_m5out'
)

record_list = gen_field_record(field_values)
filled_record_list = extract_stats(record_list, path_template, stats_names)

stats_data = pd.DataFrame.from_records(filled_record_list)
#print(stats_data)

stats_data['stride_degree'] = stats_data['stride_degree'].astype(int)
#stats_data['dcache_size'] = stats_data['dcache_size'].astype(int)
stats_data['l2cache_size'] = stats_data['l2cache_size'].astype(int)
stats_data['range_degree'] = stats_data['range_degree'].astype(int)
stats_data['range_ahead'] = stats_data['range_ahead'].astype(int)

# data filter
stats_data = stats_data[stats_data['range_degree'] == 16]
stats_data = stats_data[stats_data['l2cache_size'] == 1024]

for v_name in stats_names:
    #v = stats_data[stats_data['range_degree'] == 16].pivot(index='stride_degree', columns='l2cache_size', values=v_name)
    #v = stats_data[stats_data['stride_degree'] == 4].pivot(index='range_degree', columns='dcache_size', values=v_name)
    #v = stats_data[stats_data['dcache_size'] == 32].pivot(index='range_degree', columns='stride_degree', values=v_name)
    v = stats_data.pivot(index='range_ahead', columns='stride_degree', values=v_name)
    print(v_name)
    print(v)
    print()



