mutable struct GlobalMacroRecord
  name::String
  num_calls::Int
  body::String

  function GlobalMacroRecord(name, body)
    return new(name, 0, body)
  end
end

mutable struct LocalMacroRecord
  name::String
  num_calls::Int
  bodies::Vector{String}

  function LocalMacroRecord(name, body)
    return new(name, 0, [body])
  end
end

mutable struct UndefinedMacroCallRecord
  name::String
  line_numbers::Vector{Int}
  calls::Vector{String}
  lines::Vector{String}

  function UndefinedMacroCallRecord(name, line_number, call, line)
    return new(name, [line_number], [call], [line])
  end
end

mutable struct RunRecord
  global_macro_record_dict::Dict{String, GlobalMacroRecord}
  local_macro_record_dict::Dict{String, LocalMacroRecord}
  undefined_macro_call_dict::Dict{String, UndefinedMacroCallRecord}
  num_lines::Int

  function RunRecord()
    return new(Dict{String, GlobalMacroRecord}(),
               Dict{String, LocalMacroRecord}(),
               Dict{String, UndefinedMacroCallRecord}(),
               0)
  end
end

function report_undefined_macro_call(rec, line, macro_range, name, line_number)
  call = line[macro_range]
  if haskey(rec.undefined_macro_call_dict, name)
    push!(rec.undefined_macro_call_dict[name].line_numbers, line_number)
    push!(rec.undefined_macro_call_dict[name].calls, call)
    push!(rec.undefined_macro_call_dict[name].lines, line)
  else
    rec.undefined_macro_call_dict[name] =
      UndefinedMacroCallRecord(name, line_number, call, line)
  end
end

function find_last_macrocall(line, start=lastindex(line))
  while true
    l = findprev("<<<", line, start)
    if l === nothing
      return
    end
    r = findnext(">>>", line, l.stop)
    if r === nothing
      start = l.start-1
      continue
    end
    return l.start:r.stop
  end
end

function parse_args_indices(s)
  ans = Int[1]
  escaped = false
  for i in eachindex(s)
    if !escaped && (s[i] == ',')
      push!(ans, i+1)
      continue
    end
    if escaped && (s[i] == '\\')
      escaped = false
      continue
    end
    escaped = (s[i] == '\\')
  end
  return push!(ans, ncodeunits(s)+2)
end

function parse_arg_text(s)
  inds = parse_args_indices(s)
  ans = Array{SubString}(undef, length(inds)-1)
  for i in 1:length(inds)-1
    start = inds[i]
    stop = prevind(s, inds[i+1]-1, 1)
    ans[i] = replace(s[start:stop], "\\," => ",")
  end
  return ans
end

@enum LINE_TYPE begin
  LINE_TYPE_NORMAL
  LINE_TYPE_GLOBAL_MACRO_DEFINITION
  LINE_TYPE_LOCAL_MACRO_DEFINITION
end

function gokuro(r, w; take_record=false)
  global_macro = Dict{String, String}()
  local_macro = Dict{String, String}()

  if take_record
    rec = RunRecord()
  end

  line_number = 0
  while true
    line = readline(r, keep=true)
    if line == ""
      break
    end

    line_number += 1

    if line[end] != '\n'
      line *= "\n"
    end

    line_type = LINE_TYPE_NORMAL
    while true
      # glogal macro definition
      if startswith(line, "#+MACRO: ")
        m = match(r"#\+MACRO: (.*?) (.*?)\n", line)
        if m === nothing
          break
        end
        name = m[1]
        body = m[2]
        body = replace(body, r"(\$+)([^0-9])" => s"@\1@\2")
        global_macro[name] = body
        line_type = LINE_TYPE_GLOBAL_MACRO_DEFINITION
        if take_record
          rec.global_macro_record_dict[name] = GlobalMacroRecord(name, body)
        end
        break
      end

      # logal macro definition
      if startswith(line, "#+MACRO_LOCAL: ")
        m = match(r"#\+MACRO_LOCAL: (.*?) (.*?)\n", line)
        if m === nothing
          break
        end
        name = m[1]
        body = m[2]
        body = replace(body, r"\$([^0-9])" => s"@$@\1")
        local_macro[name] = body
        line_type = LINE_TYPE_LOCAL_MACRO_DEFINITION
        if take_record
          if haskey(rec.local_macro_record_dict, name)
            push!(rec.local_macro_record_dict[name].bodies, body)
          else
            rec.local_macro_record_dict[name] = LocalMacroRecord(name, body)
          end
        end
        break
      end

      # macro expansion
      macro_range = find_last_macrocall(line)
      if macro_range === nothing
        break
      end

      macro_prev_idx = prevind(line, macro_range.start)
      macro_next_idx = nextind(line, macro_range.stop)
      zero_or_valid(s, i) = i == 0 || isvalid(s, i)
      @assert zero_or_valid(line, macro_range.start)
      @assert zero_or_valid(line, macro_range.stop)
      @assert zero_or_valid(line, macro_prev_idx)
      @assert zero_or_valid(line, macro_next_idx)

      is_constant = !contains(SubString(line, macro_range), '(')
      if is_constant
        # macro without arguments.
        name_start = nextind(line, macro_range.start + 2) # 2 = length("<<<") - 1 = length(">>>") - 1
        name_stop = prevind(line, macro_range.stop - 2)
        name_range = name_start:name_stop
        name = SubString(line, name_range)
        if haskey(local_macro, name)
          body = local_macro[name]
          if take_record
            rec.local_macro_record_dict[name].num_calls += 1
          end
        elseif haskey(global_macro, name)
          body = global_macro[name]
          if take_record
            rec.global_macro_record_dict[name].num_calls += 1
          end
        else
          # name is not defined as macro.
          if take_record
            report_undefined_macro_call(rec, line, macro_range, name, line_number)
          end
          line = line[1:macro_prev_idx] * line[macro_next_idx:end]
          continue
        end
        body = replace(body, r"@(\$+)@" => s"\1")
        line = line[1:macro_prev_idx] * body * line[macro_next_idx:end]
        continue
      end

      # macro with arguments.
      m = match(r"<<<(.*?)\((.*?)\)>>>", SubString(line, macro_range))
      if m === nothing
        # this macro call is ill-formed.
        line = line[1:macro_prev_idx] * line[macro_next_idx:end]
        continue
      end
      name = m[1]
      if haskey(local_macro, name)
        body = local_macro[name]
        if take_record
          rec.local_macro_record_dict[name].num_calls += 1
        end
      elseif haskey(global_macro, name)
        body = global_macro[name]
        if take_record
          rec.global_macro_record_dict[name].num_calls += 1
        end
      else
        # name is not defined as macro.
        if take_record
          report_undefined_macro_call(rec, line, macro_range, name, line_number)
        end
        line = line[1:macro_prev_idx] * line[macro_next_idx:end]
        continue
      end
      arg_text = replace(m[2], "\$" => "@\$@")
      args = parse_arg_text(arg_text)
      body = replace(body, "\$0"=>arg_text)
      for (i, arg) in enumerate(args)
        body = replace(body, "\$$i"=>arg)
      end
      for i in length(args):9
        body = replace(body, "\$$i"=>"")
      end
      body = replace(body, r"@(\$+)@" => s"\1")
      line = line[1:macro_prev_idx] * body * line[macro_next_idx:end]
    end

    if line_type == LINE_TYPE_NORMAL
      print(w, line)
    end

    if line_type != LINE_TYPE_LOCAL_MACRO_DEFINITION
      empty!(local_macro)
    end
  end

  if take_record
    rec.num_lines = line_number
    return rec
  end
end

function gokuro(s; kwargs...)
  w = IOBuffer()
  rec_maybe = gokuro(IOBuffer(s), w; kwargs...)
  s = read(seekstart(w), String)
  if rec_maybe === nothing
    return s
  else
    return s, rec_maybe
  end
end

if PROGRAM_FILE != ""
  gokuro(stdin, stdout)
end
