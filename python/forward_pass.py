from emulator import emulate
import pandas as pd
import io


def get_neighboors(grid_x,grid_y):

    middle = [(grid_x,grid_y)]
    r_one = [(grid_x-1,grid_y),(grid_x+1,grid_y),(grid_x,grid_y-1),(grid_x,grid_y+1)]
    r_two = [(grid_x-2,grid_y),(grid_x+2,grid_y),(grid_x,grid_y-2),(grid_x,grid_y+2)]
    corners = [(grid_x-1,grid_y-1),(grid_x+1,grid_y+1),(grid_x+1,grid_y-1),(grid_x-1,grid_y+1)]

    return middle + r_one + corners + r_two

def get_info_position(start, grid_x,grid_y):

    with open('test_data/pairings_epoch_{}.csv'.format(start), 'r',encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Load into pandas from the fixed string
    df = pd.read_csv(io.StringIO(content))

    filtered = df[df["position_x"]==grid_x]
    filtered_df = filtered[filtered["position_y"]==grid_y]

    return filtered_df

def get_string_pairing(start, grid_x,grid_y):

    filtered = get_info_position(start, grid_x,grid_y)
    string = filtered["program"].tolist()[0]

    instructions = [",",".","[","]","{","}","<",">","+","-"]
    chars = []
    for i in string:
        if i in instructions:
            chars.append(i)
        else:
            chars.append(" ")
    return "".join(chars)

def get_prev_pos(start, grid_x,grid_y):

    filtered = get_info_position(start, grid_x,grid_y)
    comb_x = filtered["combined_x"].tolist()[0]
    comb_y = filtered["combined_y"].tolist()[0]
    return comb_x,comb_y

def check_replicator(string):

    program1 = bytearray(string.encode('utf-8'))
    program2 = bytearray(b"0" * len(program1))
    tape = program1 + program2

    tape, state, iteration, skipped = emulate(tape, 0 , len(program1), verbose=0, max_iter=1024)

    mid = len(tape) // 2
    is_repeated = tape[:mid] == tape[mid:]
    return is_repeated

def find_replicators(start,grid_x,grid_y,last):

    sf_programs = {}

    sf_programs[start] = [(grid_x,grid_y,get_string_pairing(start,grid_x,grid_y))]

    for time in range(start,last):
        sf_programs[time+1] = []
        for replicators in sf_programs[time]:

            pair_neighboor = ()
            
            for neigh_x, neigh_y in get_neighboors(replicators[0],replicators[1]):

                pair_neighboor = get_prev_pos(time+1, neigh_x, neigh_y)
                

                if (replicators[0],replicators[1]) == pair_neighboor:
                    
                    next = get_string_pairing(time+1,neigh_x,neigh_y)
                    dif = [ next[i] == replicators[2][i] for i in range(len(next)) ]
                    mod_rate = sum(dif)/len(dif)

                    #print(next)

                    if mod_rate > 0.9:
                        if check_replicator(next):
                            sf_programs[time+1].append((neigh_x,neigh_y,next))

                    next_same = get_string_pairing(time+1,replicators[0],replicators[1])
                    dif = [ next_same[i] == replicators[2][i] for i in range(len(next)) ]
                    mod_rate = sum(dif)/len(dif)

                    #print(next_same)

                    if mod_rate > 0.9:
                        if check_replicator(next_same):
                            sf_programs[time+1].append((replicators[0],replicators[1],next_same))

                
                if  pair_neighboor == (-1,-1) and (neigh_x, neigh_y) ==(replicators[0],replicators[1]):

                    next = get_string_pairing(time+1,neigh_x,neigh_y)
                    dif = [ next[i] == replicators[2][i] for i in range(len(next)) ]
                    mod_rate = sum(dif)/len(dif)

                    if mod_rate > 0.9:
                        if check_replicator(next):
                            sf_programs[time+1].append((neigh_x,neigh_y,next))
                
        print("found {} replicators in t={}".format(len(sf_programs[time+1]),time+1))

    return sf_programs

if __name__ == "__main__":
    start,grid_x,grid_y,last = 16324,14,27,16327

    rep = find_replicators(start,grid_x,grid_y,last)

    replicators = []
    for triples_list in rep.values():
        for triples in triples_list:
            replicators.append(triples[2])

    print("Total number of replicators found: {}".format(len(replicators)))