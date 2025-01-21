"""
This script processes SVG files for laser cutting.
For each svg file in the specified folder, it runs some checks and exports the files for laser cutting.

Specifically, for each .svg file it does the following:    
1. Obtain attributes of all paths in the svg file using svgpathtools (python package).
2. Obtain the dimensions of the svg file and all paths, using inkscape CLI.
3. Check that all paths from step (2) have expected attributes (from step (1)), e.g. line thickness.
4. Export pdf and text file describing dimensions to a zip file.  

Initially I tried to read information about all paths and work out the dimensions that way,
but this gets a bit complex because objects can be defined (<defs> elements) and then used
multiple times.
"""

from svgpathtools import svg2paths
import subprocess
import os
import zipfile

### INPUT PARAMETERS
input_path = r'D:\Andrew\Documents\STLs\2025-01 first full octave'
# seems to always be 96 for inkscape files in mm
dpi = 96
expected_style = {
    'fill': 'none',
    'stroke': '#000000',
    'stroke_width_mm': 0.01
}
# WARNING: this script will delete existing files in the output directory
output_path = os.path.join(input_path, 'py_output')

### FUNCTIONS
def pixels_to_mm(pixels, dpi=96):
    # 1 inch = 25.4 mm
    return pixels * 25.4 / dpi

def get_inkscape_output(file_path):
    command = f'inkscape --query-all "{file_path}"'
    result = subprocess.run(command, capture_output=True, text=True, shell=True)
    return result.stdout

def parse_inkscape_output(output):
    elements = output.strip().split('\n')
    bounding_boxes = []
    for element in elements:
        parts = element.split(',')
        if len(parts) == 5:
            element_id, x, y, width, height = parts
            bounding_boxes.append({
                'id': element_id,
                'x': float(x),
                'y': float(y),
                'width': float(width),
                'height': float(height)
            })
    return bounding_boxes


### SETUP
# read all svg files in the input directory
svg_files = [f for f in os.listdir(input_path) if f.endswith('.svg')]

# if not exist, create a new folder for the output
if not os.path.exists(output_path):
    os.makedirs(output_path)
# delete all files in output directory
for f in os.listdir(output_path):
    os.remove(os.path.join(output_path, f))

### PROCESS SVG FILES
for f in svg_files: #['e_acrylic-clear-3mm_blades-black.svg']:
    print(f)
    # list of files to add to the final zip archive
    zipfile_list = []
    file_path = os.path.join(input_path, f)

    # obtain paths and attributes using svgpathtools
    paths, attributes = svg2paths(file_path)
    id2attributes = {a['id']: {k: v for k, v in a.items() if k != 'id'} for a in attributes}

    # obtain bounding box using inkscape
    output = get_inkscape_output(file_path)
    bounding_boxes = parse_inkscape_output(output)
    id2bbox = {bbox['id']: {k: v for k, v in bbox.items() if k != 'id'} for bbox in bounding_boxes}

    # convert bounding box from pixels to mm
    for k in ['x', 'y', 'width', 'height']:
        for box in bounding_boxes:
            box[k] = pixels_to_mm(box[k], dpi)
            # account for stroke width
            box[k] -= expected_style['stroke_width_mm']

    # get the bounding box of the svg element, i.e. the overall dimensions
    svg_idx = [i for i, box in enumerate(bounding_boxes) if box['id'].startswith('svg')]

    # check there is only one svg element
    assert len(svg_idx) == 1
    svg_idx = svg_idx[0]
    svg_bbox = bounding_boxes[svg_idx]

    print(f"width: {svg_bbox['width']:.4f} mm")
    print(f"height: {svg_bbox['height']:.4f} mm")
    print()

    # validation
    for path_id, bbox in id2bbox.items():
        if path_id.startswith('path'):
            id2attributes[path_id]
            style = {s.split(':')[0]: s.split(':')[1] for s in id2attributes[path_id]['style'].split(';')}
            if 'fill' in style:
                assert style['fill'] == expected_style['fill'], f"Fill: {style['fill']}"
            if 'stroke' in style:
                assert style['stroke'] == expected_style['stroke'], f"Stroke: {style['stroke']}"
            if 'stroke-width' in style:
                stroke_width_mm = pixels_to_mm(float(style['stroke-width']), dpi)
                assert round(stroke_width_mm, 3) == expected_style['stroke_width_mm'], f"Stroke Width (mm): {stroke_width_mm}"

    # write out file info

    # # export the svg file to pdf
    # output_file = os.path.join(output_path, 'test.pdf')
    output_file = os.path.join(output_path, f'{f[:-4]}_{svg_bbox['width']:.2f}x{svg_bbox['height']:.2f}mm.pdf')
    zipfile_list.append(output_file)
    # construct the command
    # not using shell=True causes an error, nonsensical error message saying pdf isn't in allowed types
    # but pdf is listed among allowed types in the error message
    command = f'inkscape --export-type=pdf --export-filename="{output_file}" "{file_path}"'

    # run the command in cmd
    result = subprocess.run(command, capture_output=True, text=True, shell=True)

    # add the pdf file to a zip archive
    output_zip_file = output_file[:-4] + '.zip'
    
    # write out the dimensions of the design to a text file in the zip archive
    dimensions_text = "Dimensions of the design:\n"
    dimensions_text += f"width: {svg_bbox['width']:.2f} mm (largest distance on X axis between two cutting lines)\n"
    dimensions_text += f"height: {svg_bbox['height']:.2f} mm (largest distance on Y axis between two cutting lines)\n"
    dimensions_text += "\nDimensions rounded to 2 decimal places."

    with zipfile.ZipFile(output_zip_file, 'a') as zipf:
        for file in zipfile_list:
            zipf.write(file, arcname=os.path.basename(file))
        zipf.writestr(
            'dimensions.txt', dimensions_text)

    
