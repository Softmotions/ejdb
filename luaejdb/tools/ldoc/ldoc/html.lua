------ generating HTML output ---------
-- Although this can be generalized for outputting any format, since the template
-- is language-agnostic, this implementation concentrates on HTML.
-- This does the actual generation of HTML, and provides support functions in the ldoc
-- table for the template
--
-- A fair amount of the complexity comes from operating in two basic modes; first, where
-- there is a number of modules (classic LuaDoc) or otherwise, where there is only one
-- module and the index contains the documentation for that module.
--
-- Like LuaDoc, LDoc puts similar kinds of documentation files in their own directories.
-- So module docs go into 'modules/', scripts go into 'scripts/', and so forth. LDoc
-- generalizes the idea of these project-level categories and in fact custom categories
-- can be created (refered to as 'kinds' in the code)

local List = require 'pl.List'
local utils = require 'pl.utils'
local path = require 'pl.path'
local stringx = require 'pl.stringx'
local template = require 'pl.template'
local tools = require 'ldoc.tools'
local markup = require 'ldoc.markup'
local prettify = require 'ldoc.prettify'
local doc = require 'ldoc.doc'
local html = {}


local quit = utils.quit

local function cleanup_whitespaces(text)
   local lines = stringx.splitlines(text)
   for i = 1, #lines do
      lines[i] = stringx.rstrip(lines[i])
   end
   lines[#lines + 1] = "" -- Little trick: file should end with newline
   return table.concat(lines, "\n")
end

local function get_module_info(m)
   local info = {}
   for tag in doc.module_info_tags() do
      local val = m.tags[tag]
      if type(val)=='table' then
         val = table.concat(val,',')
      end
      tag = stringx.title(tag)
      info[tag] = val
   end
   if next(info) then
      return info
   end
end

local escape_table = { ["'"] = "&apos;", ["\""] = "&quot;", ["<"] = "&lt;", [">"] = "&gt;", ["&"] = "&amp;" }

function html.generate_output(ldoc, args, project)
   local check_directory, check_file, writefile = tools.check_directory, tools.check_file, tools.writefile

   function ldoc.escape(str)
      return (str:gsub("['&<>\"]", escape_table))
   end

   function ldoc.prettify(str)
      return prettify.code('lua','usage',str,0,false)
   end

   -- Item descriptions come from combining the summary and description fields
   function ldoc.descript(item)
      return (item.summary or '?')..' '..(item.description or '')
   end

   -- this generates the internal module/function references
   function ldoc.href(see)
      if see.href then -- explict reference, e.g. to Lua manual
         return see.href
      else
         return ldoc.ref_to_module(see.mod)..'#'..see.name
      end
   end

   -- this is either called from the 'root' (index or single module) or
   -- from the 'modules' etc directories. If we are in one of those directories,
   -- then linking to another kind is `../kind/name`; to the same kind is just `name`.
   -- If we are in the root, then it is `kind/name`.
   function ldoc.ref_to_module (mod)
      local base = "" -- default: same directory
      mod = mod or ldoc.module
      local kind, module = mod.kind, ldoc.module
      local name = mod.name -- default: name of module
      if not ldoc.single then
         if module then -- we are in kind/
            if module.type ~= type then -- cross ref to ../kind/
               base = "../"..kind.."/"
            end
         else -- we are in root: index
            base = kind..'/'
         end
      else -- single module
         if mod == ldoc.single then
            name = ldoc.output
            if not ldoc.root then base = '../' end
         elseif ldoc.root then -- ref to other kinds (like examples)
            base = kind..'/'
         else
            if module.type ~= type then -- cross ref to ../kind/
               base = "../"..kind.."/"
            end
         end
      end
      return base..name..'.html'
   end

   function ldoc.use_li(ls)
      if #ls > 1 then return '<li>','</li>' else return '','' end
   end

   function ldoc.display_name(item)
      local name = item.display_name or item.name
      if item.type == 'function' or item.type == 'lfunction' then return name..'&nbsp;'..item.args
      else return name end
   end

   function ldoc.no_spaces(s) return (s:gsub('%A','_')) end

   function ldoc.titlecase(s)
      return (s:gsub('(%a)(%a*)',function(f,r)
         return f:upper()..r
      end))
   end

   function ldoc.is_list (t)
      return type(t) == 'table' and t.append
   end

   function ldoc.typename (tp)
      if not tp or tp == '' then return '' end
      local optional
      -- ?<type> is short for ?nil|<type>
      if tp:match("^%?") and not tp:match '|' then
         tp = '?|'..tp:sub(2)
      end
      local tp2 = tp:match("%?|?(.*)")
      if tp2 then
         optional = true
         tp = tp2
      end
      local types = {}
      for name in tp:gmatch("[^|]+") do
         local ref,err = markup.process_reference(name)
         if ref then
            types[#types+1] = ('<a class="type" href="%s">%s</a>'):format(ldoc.href(ref),ref.label or name)
         else
            types[#types+1] = '<span class="type">'..name..'</span>'
         end
      end
      local names = table.concat(types, ", ", 1, math.max(#types-1, 1))
      if #types > 1 then names = names.." or "..types[#types] end
      if optional then
         if names ~= '' then
            if #types == 1 then names = "optional "..names end
         else
            names = "optional"
        end
      end
      return names
   end

   local module_template,err = utils.readfile (path.join(args.template,ldoc.templ))
   if not module_template then
      quit("template not found at '"..args.template.."' Use -l to specify directory containing ldoc.ltp")
   end

   local css = ldoc.css
   ldoc.output = args.output
   ldoc.ipairs = ipairs
   ldoc.pairs = pairs
   ldoc.print = print

   -- in single mode there is one module and the 'index' is the
   -- documentation for that module.
   ldoc.module = ldoc.single
   if ldoc.single and args.one then
      ldoc.kinds_allowed = {module = true, topic = true}
   end
   ldoc.root = true
   if ldoc.module then
      ldoc.module.info = get_module_info(ldoc.module)
   end
   local out,err = template.substitute(module_template,{
      ldoc = ldoc,
      module = ldoc.module,
    })
   ldoc.root = false
   if not out then quit("template failed: "..err) end

   check_directory(args.dir) -- make sure output directory is ok

   args.dir = args.dir .. path.sep

   check_file(args.dir..css, path.join(args.style,css)) -- has CSS been copied?

   -- write out the module index
   out = cleanup_whitespaces(out)
   writefile(args.dir..args.output..args.ext,out)

   -- in single mode, we exclude any modules since the module has been done;
   -- this step is then only for putting out any examples or topics
   local mods = List()
   for kind, modules in project() do
      local lkind = kind:lower()
      if not ldoc.single or ldoc.single and lkind ~= 'modules' then
         mods:append {kind, lkind, modules}
      end
   end

   -- write out the per-module documentation
   -- note that we reset the internal ordering of the 'kinds' so that
   -- e.g. when reading a topic the other Topics will be listed first.
   ldoc.css = '../'..css
   for m in mods:iter() do
      local kind, lkind, modules = unpack(m)
      check_directory(args.dir..lkind)
      project:put_kind_first(kind)
      for m in modules() do
         ldoc.module = m
         ldoc.body = m.body
         m.info = get_module_info(m)
         if ldoc.body and m.postprocess then
            ldoc.body = m.postprocess(ldoc.body)
         end
         out,err = template.substitute(module_template,{
            module=m,
            ldoc = ldoc
         })
         if not out then
            quit('template failed for '..m.name..': '..err)
         else
            out = cleanup_whitespaces(out)
            writefile(args.dir..lkind..'/'..m.name..args.ext,out)
         end
      end
   end
   if not args.quiet then print('output written to '..tools.abspath(args.dir)) end
end

return html

